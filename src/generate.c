/* $Id: generate.c,v 1.1 1996/11/07 08:03:36 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ソースコードジェネレートモジュール
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <time.h>		/* time ctime */
#include <ctype.h>		/* isprint */
#include <stdio.h>
#include <stdlib.h>		/* getenv */
#include <string.h>
#include <unistd.h>

#include "disasm.h"
#include "estruct.h"
#include "etc.h"	/* charout, strupr, <jctype.h> */
#include "fpconv.h"
#include "generate.h"
#include "global.h"
#include "hex.h"
#include "include.h"	/* Doscall_mac_path , etc. */
#include "label.h"	/* search_label etc. */
#include "offset.h"	/* depend_address, nearadrs */
#include "output.h"
#include "symbol.h"	/* symbol_search */
#include "table.h"

#if defined (__LIBC__)
  #include <sys/xstart.h>	/* char* _comline */
  #define COMMAND_LINE _comline
#else
  private char*		make_cmdline (char** argv);
  #define COMMAND_LINE  make_cmdline (argv + 1)
  #define NEED_MAKE_CMDLINE
#endif


/* Macros */
#define MIN_BIT(x) ((x) & -(x))

#ifdef	DEBUG
#define	DEBUG_PRINT(arg) eprintf arg
#else
#define	DEBUG_PRINT(arg)
#endif


USEOPTION option_q, option_r, option_x,
	  option_B, option_M, option_N, /* option_Q, */
	  option_S, option_U;


/* main.c */
extern	char	CommentChar;


/* static 関数プロトタイプ */
private lblbuf*		gen (char* section, address end, lblbuf* lptr, int type);
#ifndef OSKDIS
private lblbuf*		gen2(char* section, address end, lblbuf* lptr, int type);
#endif
private void		output_file_open_next (int sect);
private address		textgen (address, address);
private void		labelchange (disasm* code, operand* op);
private void		a7toSP (operand*);
private void		syntax_new_to_old (operand* op);
private address		datagen (address, address, opesize);
private void		datagen_sub (address, address, opesize);

private void		dataout (address, ULONG, opesize);
private void		byteout (address, ULONG, boolean);
private void		wordout (address, ULONG);
private void		longout (address, ULONG);
private void		quadout (address pc, ULONG byte);
private void		floatout (address pc, ULONG byte);
private void		doubleout (address pc, ULONG byte);
private void		extendout (address pc, ULONG byte);
private void		packedout (address pc, ULONG byte);

private void		strgen (address, address);
private void		relgen (address, address);
private void		rellonggen (address, address);
private void		byteout_for_moption (address, char*, int);
private void		zgen (address, address);
private address		tablegen (address, address, lblmode);
private address		tablegen_sub (formula*, address, address, int);
private void		tablegen_label (address, address, lblbuf*);
private void		tablegen_dc (address, lblbuf*, char*, int);
private void		bssgen (address, address, opesize);
private void		label_line_out (address adrs, lblmode mode);
private void		label_line_out_last (address adrs, lblmode mode);
private void		label_op_out (address);
private void		output_opecode (char*);
private char*		mputype_numstr (mputypes m);
private void		makeheader (char*, time_t, char*, char*);

#ifdef	OSKDIS
private void		oskdis_gen (void);
private lblbuf*		idatagen (address end, lblbuf* lptr);
private void		vlabelout (address, lblmode);
private void		wgen (address pc, address pcend);
#endif	/* OSKDIS */


int	SymbolColonNum = 2;
short	sp_a7_flag;		/* a7 を sp と出力するか? */
short	Old_syntax;

int Xtab = 7;			/* /x option TAB level */
int Atab = 8;			/* /a option TAB level */
int Mtab = 5;			/* /M option TAB level */

int Data_width = 8;		/* データ領域の横バイト数 */
int String_width = 60;		/* 文字列領域の横バイト数 */
int Compress_len = 64;		/* データ出力をdcb.?にする最小バイト数 */

size_mode Generate_SizeMode = SIZE_NORMAL;

char	Label_first_char = 'L';	/* ラベルの先頭文字 */

char	opsize[] = "bwlqssdxp";

static mputypes	Current_Mputype;
static UWORD	SectionType;


#ifdef	OSKDIS
char*	OS9label [0x100];	/* os9	 F$???(I$???)	*/
char*	MATHlabel[0x100];	/* tcall T$Math,T$???	*/
char*	CIOlabel [0x100];	/* tcall CIO$Trap,C$??? */
#else
char**	IOCSlabel;
char	IOCSCallName[16] = "IOCS";
const char*	Header_filename;
#endif	/* OSKDIS */


/* 擬似命令 */

#ifdef	OSKDIS
#define PSEUDO	"\t"
#define EVEN	"align"
#else
#define PSEUDO	"\t."
#define EVEN	"even"
#define DCB_B	"dcb.b\t"
#define DCB_W	"dcb.w\t"
#define DCB_L	"dcb.l\t"
#define DCB_Q	"dcb.d\t"
#define DCB_S	"dcb.s\t"
#define DCB_D	"dcb.d\t"
#define DCB_X	"dcb.x\t"
#define DCB_P	"dcb.p\t"
#endif	/* !OSKDIS */

#define INCLUDE	"include\t"
#define CPU	"cpu\t"
#define FPID	"fpid\t"
#define EQU	"equ\t"
#define XDEF	"xdef\t"

#define DC_B	"dc.b\t"
#define DC_W	"dc.w\t"
#define DC_L	"dc.l\t"
#define DC_Q	"dc.d\t"
#define DC_S	"dc.s\t"
#define DC_D	"dc.d\t"
#define DC_X	"dc.x\t"
#define DC_P	"dc.p\t"

#define DS_B	"ds.b\t"
#define DS_W	"ds.w\t"
#define DS_L	"ds.l\t"

#define DEFAULT_FPID	1


/* Inline Functions */

static INLINE char*
strcpy2 (char* dst, char* src)
{
    while ((*dst++ = *src++) != 0)
	;
    return --dst;
}


/*

  Check displacement of Relative branch

  return FALSE;      short    で済む displacement が word の場合
	もしくは、short||word で済む displacement が long の場合
  return TRUE; サイズが省略可能な場合.

  as.xのバグへの対応は削除(1997/10/10).
  jmp,jsrはcode.size==NOTHINGである為、無関係(呼び出しても無害).

*/

static INLINE boolean
check_displacement (address pc, disasm* code)
{
    long dist = (ULONG)code->jmp - (ULONG)pc;
    unsigned char c = code->opecode[0];

    /* bra, bsr, bcc */
    if (c == 'b') {
	if (code->size == WORDSIZE)
	    return ((dist <= 127) && (-dist <= 128 + 2)) ? FALSE : TRUE;
	if (code->size == LONGSIZE)
	    return ((dist <= 32767) && (-dist <= 32768 + 2)) ? FALSE : TRUE;
	return TRUE;
    }

    /* dbcc, fdbcc, pdbcc : always 16bit displacement */
    if ((c == 'd') || (code->opecode[1] == (char)'d')) {
	return TRUE;
    }

    /* fbcc, pbcc : 16bit or 32bit displacement */
    {
	if (code->size == LONGSIZE)
	    return ((dist <= 32767) && (-dist <= 32768 + 2)) ? FALSE : TRUE;
	return TRUE;
    }

    /* NOT REACHED */
}


/*

  アセンブリソースを出力する

  input:
    xfilename	: execute file name
    sfilename	: output file name
    filedate	: execute file' time stamp
    argc	: same to argc given to main()	// 現在未使用
    argv	: same to argv given to main()

*/
extern void
generate (char* xfilename, char* sfilename,
		time_t filedate, int argc, char* argv[])
{

    /* ニーモニックは必要 */
    Disasm_String = TRUE;
    Current_Mputype = MIN_BIT (MPU_types);

    if (option_x)
	Atab += 2;

    init_output ();
    output_file_open (sfilename, 0);

#ifdef __LIBC__
    /* 標準出力に書き込む場合、標準エラー出力と出力先が同じ場合は */
    /* ソースコード出力中の表示を抑制する */
    if (is_confuse_output ()) {
	eputc ('\n');
	option_q = TRUE;
    }
#endif

    makeheader (xfilename, filedate, argv[0], COMMAND_LINE);

#ifdef	OSKDIS
    oskdis_gen ();
#else
    {
	lblbuf* lptr = next (BeginTEXT);

	lptr = gen  ("\t.text"	CR CR, BeginDATA,  lptr, XDEF_TEXT);
	lptr = gen  ("\t.data"	CR CR, BeginBSS,   lptr, XDEF_DATA);
	lptr = gen2 ("\t.bss"	CR CR, BeginSTACK, lptr, XDEF_BSS);
	lptr = gen2 ("\t.stack" CR CR, Last,	   lptr, XDEF_STACK);

	/* 末尾にある属性 0 のシンボルを出力する */
	SectionType = (UWORD)0;
	label_line_out_last (lptr->label, lptr->mode);

	output_opecode (CR "\t.end\t");
	label_op_out (Head.exec);
    }
#endif	/* !OSKDIS */

    newline (Last);
    output_file_close ();
}


/*

  １セクションを出力する

  input:
    section : section name ; ".text" or ".data"
    end     : block end address
    lptr    : lblbuf* ( contains block start address )

*/
private lblbuf*
gen (char* section, address end, lblbuf* lptr, int type)
{
    address  pc = lptr->label;
    lblmode  nmode = lptr->mode;

    /* 0 バイトのセクションで、かつシンボルが存在しなければ */
    /* セクション疑似命令も出力しない.			    */
    if (pc == end && !symbol_search2 (pc, type))
	return lptr;

    /* 必要なら出力ファイルを *.dat に切り換える */
    output_file_open_next (type);

    outputa (CR);
    output_opecode (section);
    SectionType = (UWORD)type;

    while (pc < end) {
	lblmode  mode = nmode;
	address  nlabel;

	do {
	    lptr = Next (lptr);
	} while (lptr->shift);
	nlabel = lptr->label;
	nmode  = lptr->mode;

	label_line_out (pc, mode);

	if (isPROLABEL (mode)) {
	    textgen (pc, nlabel);
	    pc = nlabel;
	} else if (isTABLE (mode)) {
	    pc = tablegen (pc, nlabel, mode);
	    lptr = next (pc);
	    nmode = lptr->mode;
	} else {
	    datagen (pc, nlabel, mode & 0xff);
	    pc = nlabel;
	}
    }

    /* セクション末尾のラベル(外部定義シンボルのみ) */
    label_line_out_last (pc, nmode);

    return lptr;
}

#ifndef	OSKDIS
/*

    .bss/.stack を出力する

*/
private lblbuf*
gen2 (char* section, address end, lblbuf* lptr, int type)
{
    address  pc = lptr->label;
    lblmode  nmode = lptr->mode;

    /* 0 バイトのセクションで、かつシンボルが存在しなければ */
    /* セクション疑似命令も出力しない.			    */
    if (pc == end && !symbol_search2 (pc, type))
	return lptr;

    /* 必要なら出力ファイルを *.bss に切り換える */
    output_file_open_next (type);

    outputa (CR);
    output_opecode (section);
    SectionType = (UWORD)type;

    while (pc < end) {
	lblmode  mode = nmode;
	address  nlabel;

	lptr   = Next (lptr);
	nlabel = lptr->label;
	nmode  = lptr->mode;

	label_line_out (pc, mode);

	if (isTABLE (mode)) {
	    pc = tablegen (pc, nlabel, mode);
	    lptr = next (pc);
	    nmode = lptr->mode;
	} else {
	    bssgen (pc, nlabel, mode & 0xff);
	    pc = nlabel;
	}
    }

    /* セクション末尾のラベル(外部定義シンボルのみ) */
    label_line_out_last (pc, nmode);

    return lptr;
}


/*

  必要なら出力ファイルを切り換える.

*/
private void
output_file_open_next (int sect)
{
    static int now = 0;			/* 最初は .text */
    int type;

    if (!option_S || sect == XDEF_TEXT)
	return;

    /* .data は -1、.bss 及び .stack は -2 */
    type = (sect >= XDEF_BSS) ? -2 : -1;

    if (now != type) {
	now = type;
	output_file_close ();
	output_file_open (NULL, now);
    }
}

#endif	/* !OSKDIS */


/*
  -M、-x オプションのタブ出力
  input:
    tabnum : Mtab or Xtab
    buffer : pointer to output buffer
  return:
    pointer to end of output buffer
*/

static INLINE char*
tab_out (int tab, char* buf)
{
    int len;
    unsigned char c;

    /* 行末までの長さを数える */
    for (len = 0; (c = *buf++) != '\0'; len++)
	if (c == '\t')
	    len |= 7;
    buf--;
    len /= 8;			/* 現在のタブ位置 */

    /* 最低一個のタブを出力する分、ループ回数を 1 減らす */
    tab--;

    /* now, buffer points buffer tail */
    for (tab -= len; tab > 0; tab--)
	*buf++ = '\t';

    *buf++ = '\t';
    *buf++ = CommentChar;
    return buf;
}


/*

  テキストブロックを出力する

  input :
    pc : text begin address
    pcend : text end address

  return :
    pcend

*/
private address
textgen (address pc, address pcend)
{
    short   made_to_order = (!sp_a7_flag || Old_syntax || option_U);
    address store = pc + Ofst;

    charout ('.');
    PCEND = pcend;

    while (pc < pcend) {
	static char prev_fpuid = DEFAULT_FPID;
	disasm code;
	opesize size;
	address store0 = store;
	address pc0 = pc;
	char  buffer[128];
	char* ptr;
	char* l;			/* IOCS コール名 */

	store += dis (store, &code, &pc);

	/* .cpu 切り換え */
	if ((code.mputypes & Current_Mputype) == 0) {
	    Current_Mputype = MIN_BIT (code.mputypes & MPU_types);
	    output_opecode (PSEUDO CPU);
	    outputa (mputype_numstr (Current_Mputype));
	    newline (pc0);
	}

	/* .fpid 切り換え */
	if ((code.fpuid >= 0) && (code.fpuid != prev_fpuid)) {
	    prev_fpuid = code.fpuid;
	    output_opecode (PSEUDO FPID);
	    outputfa ("%d", (int)code.fpuid);
	    newline (pc0);
	}

	/* code.size はサイズ省略時に NOTHING に書き換えられるので */
	/* 即値のコメント出力用に、本当のサイズを保存しておく	   */
	size = code.size;

	if (code.flag == OTHER) {
	    if (size == code.default_size && option_N)
		code.size = NOTHING;
	}
	else if (code.jmpea == PCDISP) {
	    if ((Generate_SizeMode == SIZE_NORMAL && size != NOTHING
		    && check_displacement (pc, &code))	/* -b0: 可能ならサイズ省略 */
		|| Generate_SizeMode == SIZE_OMIT)	/* -b1: 常にサイズ省略 */
		code.size = NOTHING;
	}

	if (made_to_order)
	    modify_operand (&code);
	else {				/* "sp"、小文字、新表記なら特別扱い */
	    if (code.op1.operand[0]) {
		a7toSP (&code.op1);
		labelchange (&code, &code.op1);

		if (code.op2.operand[0]) {
		    a7toSP (&code.op2);
		    labelchange (&code, &code.op2);

		    if (code.op3.operand[0]) {
			a7toSP (&code.op3);
			labelchange (&code, &code.op3);

			if (code.op4.operand[0]) {
			    a7toSP (&code.op4);
			    labelchange (&code, &code.op4);
			}
		    }
		}
	    }
	}

	ptr = buffer;
	*ptr++ = '\t';

#ifdef	OSKDIS
#define OS9CALL(n, v, t)  (peekw (store0) == (n) && (v) < 0x100 && (l = (t)[(v)]))
	if (OS9CALL (0x4e40, code.op1.opval, OS9label))		/* trap #0 (OS9) */
	    strcat (strcpy (ptr, "OS9\t"), l);
	else if (OS9CALL (0x4e4d, code.op2.opval, CIOlabel))	/* trap #13 (CIO$Trap) */
	    strcat (strcpy (ptr, "TCALL\tCIO$Trap,"), l);
	else if (OS9CALL (0x4e4f, code.op2.opval, MATHlabel))	/* trap #15 (T$Math) */
	    strcat (strcpy (ptr, "TCALL\tT$Math,"), l);
#else
	if (*(UBYTE*)store0 == 0x70		/* moveq #imm,d0 + trap #15 なら */
	 && peekw (store) == 0x4e4f		/* IOCS コールにする		 */
	 && IOCSlabel && (l = IOCSlabel[*(UBYTE*)(store0 + 1)]) != NULL
	 && (pc < pcend)) {
	    ptr = strcpy2 (ptr, IOCSCallName);
	    *ptr++ = '\t';
	    strcpy2 (ptr, l);
	    store += 2;
	    pc += 2;
	    code.opflags &= ~FLAG_NEED_COMMENT;	/* moveq のコメントは付けない */
	}
#endif	/* OSKDIS */
	else {
	    ptr = strcpy2 (ptr, code.opecode);
	    if (code.size < NOTHING) {
		*ptr++ = '.';
		*ptr++ = opsize[code.size];
	    }

	    if (code.op1.operand[0]) {
		*ptr++ = '\t';
		ptr = strcpy2 (ptr, code.op1.operand);

		if (code.op2.operand[0]) {
		    if (code.op2.ea != BitField && code.op2.ea != KFactor)
			*ptr++ = ',';
		    ptr = strcpy2 (ptr, code.op2.operand);

		    if (code.op3.operand[0]) {
			if (code.op3.ea != BitField && code.op3.ea != KFactor)
			    *ptr++ = ',';
			ptr = strcpy2 (ptr, code.op3.operand);

			if (code.op4.operand[0]) {
			    if (code.op4.ea != BitField && code.op4.ea != KFactor)
				*ptr++ = ',';
			    strcpy2 (ptr, code.op4.operand);
			}
		    }
		}
	    }
	}

	if (code.flag == UNDEF) {	/* -M と同じ桁に出力 */
	    strcpy (tab_out (Mtab, buffer), "undefined inst.");
	}

	else if ((code.opflags & FLAG_NEED_COMMENT) && option_M)
	    byteout_for_moption (pc0, buffer, size);

	if (option_x)
	    byteout_for_xoption (pc0, pc - pc0, buffer);

	outputa (buffer);
	newline (pc0);

	/* rts、jmp、bra の直後に空行を出力する		*/
	/* ただし、-B オプションが無指定なら bra は除く	*/
	if ((code.opflags & FLAG_NEED_NULSTR)
	 && (option_B || (code.opecode[0] != 'b' && code.opecode[0] != 'B')))
	    outputa (CR);

    }

    return pcend;
}



/*

	出力形式の指定に従ってオペランドを修正する.

	1)レジスタ名"a7"を"sp"にする("za7"も"zsp"にする).
	2)オペコード及び各オペランドを大文字にする.
	3)ラベルに変更できる数値はラベルにする.
	4)アドレッシング表記を古い書式にする.

*/

extern void
modify_operand (disasm *code)
{
    int upper_flag = (option_U && !(code->opflags & FLAG_CANNOT_UPPER));

    if (upper_flag)
	strupr (code->opecode);

    if (code->op1.operand[0]) {
	if (sp_a7_flag) a7toSP (&code->op1);
	if (upper_flag) strupr (code->op1.operand);
	labelchange (code, &code->op1);
	if (Old_syntax) syntax_new_to_old (&code->op1);

	if (code->op2.operand[0]) {
	    if (sp_a7_flag) a7toSP (&code->op2);
	    if (upper_flag) strupr (code->op2.operand);
	    labelchange (code, &code->op2);
	    if (Old_syntax) syntax_new_to_old (&code->op2);

	    if (code->op3.operand[0]) {
		if (sp_a7_flag) a7toSP (&code->op3);
		if (upper_flag) strupr (code->op3.operand);
		labelchange (code, &code->op3);
		if (Old_syntax) syntax_new_to_old (&code->op3);

		if (code->op4.operand[0]) {
		    if (sp_a7_flag) a7toSP (&code->op4);
		    if (upper_flag) strupr (code->op4.operand);
		    labelchange (code, &code->op4);
		    if (Old_syntax) syntax_new_to_old (&code->op4);
		}
	    }
	}
    }
}


/*

　a7 表記を sp 表記に変える

*/
private void
a7toSP (operand* op)
{
    char *ptr = op->operand;

    switch (op->ea) {
    case AregD:
	break;
    case AregID:
    case AregIDPI:
	ptr++;
	break;
    case AregIDPD:
	ptr += 2;
	break;
    case AregDISP:
	ptr = strchr (ptr, ',') + 1;
	break;
    case AregIDX:
	ptr = strrchr (ptr, ',') - 2;
	if (ptr[1] == '7') {
	    ptr[0] = 's';
	    ptr[1] = 'p';
	}
	ptr += 3;
	break;
    case PCIDX:
	ptr = strrchr (ptr, ',') + 1;
	break;

    case RegPairID:
	ptr++;
	if (ptr[0] == 'a' && ptr[1] == '7') {
	    ptr[0] = 's';
	    ptr[1] = 'p';
	}
	ptr += 5;
	break;

    case PCIDXB:
    case PCPOSTIDX:
    case PCPREIDX:
    case AregIDXB:
    case AregPOSTIDX:
    case AregPREIDX:
	if (*++ptr == '[')
	    ptr++;
	while (1) {
	    if (*ptr == 'z')
		ptr++;
	    if (ptr[0] == 'a' && ptr[1] == '7') {
		*ptr++ = 's';
		*ptr++ = 'p';
	    }
	    if ((ptr = strchr (ptr, ',')) == NULL)
		return;
	    ptr++;
	}
	return;

    default:
	return;
    }

    if (ptr[0] == 'a' && ptr[1] == '7') {
	*ptr++ = 's';
	*ptr++ = 'p';
    }

}


/*

	アドレッシング形式を旧式の表記に変える.

*/

private void
syntax_new_to_old (operand *op)
{
    char *optr = op->operand;
    char *p;

    switch (op->ea) {
	case AbShort:			/* (abs)[.w] -> abs[.w] */
	case AbLong:			/* (abs)[.l] -> abs[.l] */
	    p = strchr (optr, ')');
	    strcpy (p, p + 1);
	    strcpy (optr, optr + 1);
	    break;

	case AregIDX:			/* (d8,an,ix) -> d8(an,ix) */
	    if (optr[1] == 'a' || optr[1] == 'A')
		break;			/* ディスプレースメント省略 */
	    /* fall through */
	case AregDISP:			/* (d16,an)   -> d16(an)   */
	case PCIDX:			/* (d8,pc,ix) -> d8(pc,ix) */
	case PCDISP:			/* (d16,pc)   -> d16(pc)   */
	    if ((p = strchr (optr, ',')) == NULL)
		break;			/* 相対分岐命令 */
	    *p = '(';
	    strcpy (optr, optr + 1);
	    break;

	default:
	    break;
    }
}



/*

  データブロックを出力する

  input :
    pc : data begin address
    pcend : data end address
    size : data size

  return :
    pcend

*/
private address
datagen (address pc, address pcend, opesize size)
{
    address next_adrs;

    while (pc < pcend) {
	if ((next_adrs = nearadrs (pc)) < pcend) {
	    if (next_adrs != pc) {
		datagen_sub (pc, next_adrs, size);
		pc = next_adrs;
	    }
	    output_opecode (PSEUDO DC_L);
	    label_op_out ((address) peekl (next_adrs + Ofst));
	    newline (next_adrs);
	    pc += 4;
	}
	else {
	    datagen_sub (pc, pcend, size);
	    pc = pcend;
	}
    }
    return pcend;
}


/*

  データブロックを出力する
  条件 : pc から pcend までにはアドレスに依存しているところはない

  input :
    pc : data begin address
    pcend : data end address
    size : data size

*/
private void
datagen_sub (address pc, address pcend, opesize size)
{
    switch (size) {
    case STRING:
	strgen (pc, pcend);
	break;
    case RELTABLE:
	relgen (pc, pcend);
	break;
    case RELLONGTABLE:
	rellonggen (pc, pcend);
	break;
    case ZTABLE:
	zgen (pc, pcend);
	break;
#ifdef	OSKDIS
    case WTABLE:
	wgen (pc, pcend);
	break;
#endif	/* OSKDIS */
    default:
	dataout (pc, pcend - pc, size);
	break;
    }
}


/*

  データブロックの出力

  input :
    pc : data begin address
    byte : number of bytes
    size : data size ( UNKNOWN , BYTESIZE , WORDSIZE , LONGSIZE only )

*/
private void
dataout (address pc, ULONG byte, opesize size)
{
    address store = pc + Ofst;
    int odd_flag = (int)store & 1;

    DEBUG_PRINT (("dataout : store %x byte %x size %x\n", store, byte, size));
    charout ('#');

    switch (size) {
	case WORDSIZE:
	    if (odd_flag || (byte % sizeof (UWORD)))
		break;
	    if (byte == sizeof (UWORD)) {
		output_opecode (PSEUDO DC_W);
#if 0
		outputax4_without_0supress (peekw (store));
#else
		outputaxd (peekw (store), 4);
#endif
		newline (pc);
	    } else
		wordout (pc, byte);
	    return;

	case LONGSIZE:
	    if (odd_flag || (byte % sizeof (ULONG)))
		break;
	    if (byte == sizeof (ULONG)) {
		output_opecode (PSEUDO DC_L);
#if 0
		outputax8_without_0supress (peekl (store));
#else
		outputaxd (peekl (store), 8);
#endif
		newline (pc);
	    } else
		longout (pc, byte);
	    return;

	case QUADSIZE:
	    if (odd_flag || (byte % sizeof (quadword)))
		break;
	    quadout (pc, byte);
	    return;

	case SINGLESIZE:
	    if (odd_flag || (byte % sizeof (float)))
		break;
	    floatout (pc, byte);
	    return;

	case DOUBLESIZE:
	    if (odd_flag || (byte % sizeof (double)))
		break;
	    doubleout (pc, byte);
	    return;

	case EXTENDSIZE:
	    if (odd_flag || (byte % sizeof (long double)))
		break;
	    extendout (pc, byte);
	    return;

	case PACKEDSIZE:
	    if (odd_flag || (byte % sizeof (packed_decimal)))
		break;
	    packedout (pc, byte);
	    return;

	case BYTESIZE:
	    if (byte == sizeof (UBYTE)) {
		output_opecode (PSEUDO DC_B);
#if 0
		outputax2_without_0supress (*(UBYTE*)store);
#else
		outputaxd (*(UBYTE*)store, 2);
#endif
		newline (pc);
		return;
	    }
	    break;

	default:
	    break;
    }

    byteout (pc, byte, FALSE);
}


/*

  データブロックの出力

  input :
    pc : data begin address
    byte : number of bytes
    roption_flag : hex comment to string flag
    ( case this function is used for string output )

*/
private void
byteout (address pc, ULONG byte, boolean roption_flag)
{
    address store = pc + Ofst;

#ifndef OSKDIS	/* OS-9/68K の r68 には dcb に相当する疑似命令が無い */
    if (!roption_flag
     && (byte >= 1 * 2) && Compress_len && (byte >= Compress_len)) {
	UBYTE* ptr = (UBYTE*) store;
	int val = *ptr;

	while (ptr < store + byte && *ptr == val)
	    ptr++;
	if (ptr == store + byte) {
	    if (val == 0) {
		output_opecode (PSEUDO DS_B);
		outputfa ("%d", byte);
	    } else {
		output_opecode (PSEUDO DCB_B);
		outputfa ("%d,", byte);
		outputax2_without_0supress (val);
	    }
	    newline (pc);
	    return;
	}
    }
#endif	/* !OSKDIS */

    {
	ULONG max = (byte - 1) / Data_width;
	ULONG mod = (byte - 1) % Data_width;
	int i, j;

	for (i = 0; i <= max; i++) {
	    if (roption_flag) {
		static char comment[] = ";\t\t";
		comment[0] = CommentChar;
		outputa (comment);
	    } else
		output_opecode (PSEUDO DC_B);

	    for (j = 1; j < (i == max ? mod + 1 : Data_width); j++) {
		outputax2_without_0supress (*store++);
		outputca (',');
	    }
	    outputax2_without_0supress (*store++);
	    newline (pc);
	    pc += Data_width;
	}
    }
}


private void
wordout (address pc, ULONG byte)
{
    address store = pc + Ofst;

#ifndef OSKDIS
    if ((byte >= sizeof (UWORD) * 2) && Compress_len && (byte >= Compress_len)) {
	UWORD* ptr = (UWORD*) store;
	int val = peekw (ptr);

	while ((address) ptr < store + byte && peekw (ptr) == val)
	    ptr++;
	if ((address) ptr == store + byte) {
	    if (val == 0) {
		output_opecode (PSEUDO DS_W);
		outputfa ("%d", byte / sizeof (UWORD));
	    } else {
		output_opecode (PSEUDO DCB_W);
		outputfa ("%d,", byte / sizeof (UWORD));
		outputax4_without_0supress (val);
	    }
	    newline (pc);
	    return;
	}
    }
#endif	/* !OSKDIS */

    {
	int datawidth = (Data_width + sizeof (UWORD) - 1) / sizeof (UWORD);
	ULONG max = (byte / sizeof (UWORD) - 1) / datawidth;
	ULONG mod = (byte / sizeof (UWORD) - 1) % datawidth;
	int i, j;

	for (i = 0; i <= max; i++) {
	    output_opecode (PSEUDO DC_W);
	    for (j = 1; j < (i == max ? mod + 1 : datawidth); j++) {
		outputax4_without_0supress (peekw (store));
		store += sizeof (UWORD);
		outputca (',');
	    }
	    outputax4_without_0supress (peekw (store));
	    store += sizeof (UWORD);
	    newline (pc);
	    pc += datawidth * sizeof (UWORD);
	}
    }
}


private void
longout (address pc, ULONG byte)
{
    address store = pc + Ofst;

#ifndef OSKDIS
    if ((byte >= sizeof (ULONG) * 2) && Compress_len && (byte >= Compress_len)) {
	ULONG* ptr = (ULONG*) store;
	int val = peekl (ptr);

	while ((address) ptr < store + byte && peekl (ptr) == val )
	    ptr++;
	if ((address) ptr == store + byte) {
	    if (val == 0) {
		output_opecode (PSEUDO DS_L);
		outputfa ("%d", byte / sizeof (ULONG));
	    } else {
		output_opecode (PSEUDO DCB_L);
		outputfa ("%d,", byte / sizeof (ULONG));
		outputax8_without_0supress (val);
	    }
	    newline (pc);
	    return;
	}
    }
#endif	/* !OSKDIS */

    {
	int datawidth = (Data_width + sizeof (ULONG) - 1) / sizeof (ULONG);
	ULONG max = (byte / sizeof (ULONG) - 1) / datawidth;
	ULONG mod = (byte / sizeof (ULONG) - 1) % datawidth;
	int i, j;

	for (i = 0; i <= max; i++) {
	    output_opecode (PSEUDO DC_L);
	    for (j = 1; j < (i == max ? mod + 1 : datawidth); j++) {
		outputax8_without_0supress (peekl(store));
		store += sizeof (ULONG);
		outputca (',');
	    }
	    outputax8_without_0supress (peekl (store));
	    store += sizeof (ULONG);
	    newline (pc);
	    pc += datawidth * sizeof (ULONG);
	}
    }
}


private void
quadout (address pc, ULONG byte)
{
    quadword* store = (quadword*)(pc + Ofst);
    char buf[64];
    int i = (byte / sizeof(quadword));

#ifndef OSKDIS
    if ((i >= 2) && Compress_len && (byte >= Compress_len)
		 && (memcmp (store, store + 1, byte - sizeof(quadword)) == 0)) {
	fpconv_q (buf, store);
	output_opecode (PSEUDO DCB_Q);
	outputfa ("%d,%s", i, buf);
	newline (pc);
	return;
    }
#endif

    do {
	fpconv_q (buf, store++);
	output_opecode (PSEUDO DC_Q);
	outputa (buf);
	newline (pc);
	pc += sizeof(quadword);
    } while (--i);
}


private void
floatout (address pc, ULONG byte)
{
    float* store = (float*)(pc + Ofst);
    char buf[64];
    int i = (byte / sizeof(float));

#ifndef OSKDIS
    if ((i >= 2) && Compress_len && (byte >= Compress_len)
		 && (memcmp (store, store + 1, byte - sizeof(float)) == 0)) {
	fpconv_s (buf, store);
	output_opecode (PSEUDO DCB_S);
	outputfa ("%d,%s", i, buf);
	newline (pc);
	return;
    }
#endif

    do {
	fpconv_s (buf, store++);
	output_opecode (PSEUDO DC_S);
	outputa (buf);
	newline (pc);
	pc += sizeof(float);
    } while (--i);
}


private void
doubleout (address pc, ULONG byte)
{
    double* store = (double*)(pc + Ofst);
    char buf[64];
    int i = (byte / sizeof(double));

#ifndef OSKDIS
    if ((i >= 2) && Compress_len && (byte >= Compress_len)
		 && (memcmp (store, store + 1, byte - sizeof(double)) == 0)) {
	fpconv_d (buf, store);
	output_opecode (PSEUDO DCB_D);
	outputfa ("%d,%s", i, buf);
	newline (pc);
	return;
    }
#endif

    do {
	fpconv_d (buf, store++);
	output_opecode (PSEUDO DC_D);
	outputa (buf);
	newline (pc);
	pc += sizeof(double);
    } while (--i);
}


private void
extendout (address pc, ULONG byte)
{
    long double* store = (long double*)(pc + Ofst);
    char buf[64];
    int i = (byte / sizeof(long double));

#ifndef OSKDIS
    if ((i >= 2) && Compress_len && (byte >= Compress_len)
		 && (memcmp (store, store + 1, byte - sizeof(long double)) == 0)) {
	fpconv_x (buf, store);
	output_opecode (PSEUDO DCB_X);
	outputfa ("%d,%s", i, buf);
	newline (pc);
	return;
    }
#endif

    do {
	fpconv_x (buf, store++);
	output_opecode (PSEUDO DC_X);
	outputa (buf);
	newline (pc);
	pc += sizeof(long double);
    } while (--i);
}


private void
packedout (address pc, ULONG byte)
{
    packed_decimal* store = (packed_decimal*)(pc + Ofst);
    char buf[64];
    int i = (byte / sizeof(packed_decimal));

#ifndef OSKDIS
    if ((i >= 2) && (memcmp (store, store + 1, byte - sizeof(packed_decimal)) == 0)) {
	fpconv_p (buf, store);
	output_opecode (PSEUDO DCB_P);
	outputfa ("%d,%s", i, buf);
	newline (pc);
	return;
    }
#endif

    do {
	fpconv_p (buf, store++);
	output_opecode (PSEUDO DC_P);
	outputa (buf);
	newline (pc);
	pc += sizeof(packed_decimal);
    } while (--i);
}



/*

  文字列の出力

  input :
    pc : string begin address
    pcend : string end address

*/
#ifdef	OSKDIS
#define QUOTE_CHAR	'\"'
#define QUOTE_STR	"\""
#else
#define QUOTE_CHAR	'\''
#define QUOTE_STR	"\'"
#endif	/* OSKDIS */

#define ENTERSTR() \
    if (!strmode) {						\
	if (comma) { outputa ("," QUOTE_STR); column += 2; }	\
	else { outputca (QUOTE_CHAR); column++;}		\
	strmode = TRUE;						\
    }
#define EXITSTR() \
    if (strmode) {		\
	outputca (QUOTE_CHAR);	\
	column++;		\
	strmode = FALSE;	\
    }

static INLINE boolean
is_sb (unsigned char* ptr)
{
    return (isprkana (ptr[0]) && ptr[0] != QUOTE_CHAR);
}

static INLINE boolean
is_mb_zen (unsigned char* ptr)
{
    return (iskanji (ptr[0]) && iskanji2 (ptr[1]));
}

static INLINE boolean
is_mb_han (unsigned char* ptr)
{
    if (ptr[0] == 0x80 || (0xf0 <= ptr[0] && ptr[0] <= 0xf3)) {
	if ((0x20 <= ptr[1] && ptr[1] <= 0x7e)
	 || (0x86 <= ptr[1] && ptr[1] <= 0xfd))
	    return TRUE;
    }
    return FALSE;
}

private void
strgen (address pc, address pcend)
{
    address	store = Ofst + pc;
    address	stend = Ofst + pcend;

    charout ('s');

    while (store < stend) {
	address    store0 = store;
	boolean    strmode = FALSE;
	int	column = 0;
	char	comma = 0;		/* 1 なら , を出力する */

	output_opecode (PSEUDO DC_B);

	while (column < String_width && store < stend) {

	    if (is_sb (store)) {
		/* ANK 文字 */
		ENTERSTR();
		outputca (*(UBYTE*)store++);
		column++;
	    } else if (is_mb_zen (store) && store + 1 < stend) {
		/* 二バイト全角 */
		ENTERSTR();
		outputca (*(UBYTE*)store++);
		outputca (*(UBYTE*)store++);
		column += 2;
	    } else if (is_mb_han (store) && store + 1 < stend) {
		/* 二バイト半角 */
		ENTERSTR();
		outputca (*(UBYTE*)store++);
		outputca (*(UBYTE*)store++);
		column++;
	    } else {
		unsigned char c = *(UBYTE*)store++;
		EXITSTR();
		if (comma)
		    outputca (',');
		outputax2_without_0supress (c);
		column += comma + 3;				/* ,$xx */

		/* \n または NUL なら改行する */
		if (store != stend && (c == '\n' || c == '\0')) {
		    /* ただし、後に続く NUL は全て一行に納める */
		    if (*(UBYTE*)store == '\0')
			;
		    else
			break;					/* 改行 */
		}
	    }
	    comma = 1;
	}
	if (strmode)
	    outputca (QUOTE_CHAR);

	newline (store0 - Ofst);
	if (option_r)
	    byteout (store0 - Ofst, store - store0, TRUE);
    }
}
#undef	ENTERSTR
#undef	EXITSTR
#undef	QUOTE_CHAR


/*

  リラティブオフセットテーブルの出力

  input :
    pc : relative offset table begin address
    pcend : relative offset table end address

*/
private void
relgen (address pc, address pcend)
{
    char buf[256], tabletop[128];
    char* bufp;
    const address pc0 = pc;

    charout ('r');

    /* 予めバッファの先頭に .dc.w を作成しておく */
    bufp = strend (strcpy (buf, PSEUDO DC_W));
    if (option_U)
	strupr (buf);

    /* -Lxxxxxx も作成しておく */
    tabletop[0] = '-';
    make_proper_symbol (&tabletop[1], pc0);

    while (pc < pcend) {
	int dif = (int)(signed short) peekw (pc + Ofst);
	char* p;

	if ((LONG) (pc0 + dif) < (LONG) BeginTEXT) {
	    make_proper_symbol (bufp, BeginTEXT);
	    p = strend (bufp);
	    *p++ = '-';
	    p = itox6d (p, BeginTEXT - (pc0 + dif));
	} else if ((LONG) (pc0 + dif) > (LONG) Last) {
	    make_proper_symbol (bufp, Last);
	    p = strend (bufp);
	    *p++ = '+';
	    p = itox6d (p, (pc0 + dif) - Last);
	} else {
	    make_proper_symbol (bufp, pc0 + dif);
	    p = strend (bufp);
	}

	strcpy (p, tabletop);

	/* 相対テーブルも -x でコメントを出力する */
	if (option_x)
	    byteout_for_xoption (pc, sizeof (WORD), buf);

	outputa (buf);
	newline (pc);

	pc += sizeof (WORD);
    }
}


/*

  ロングワードなリラティブオフセットテーブルの出力

  input :
    pc : relative offset table begin address
    pcend : relative offset table end address

*/
private void
rellonggen (address pc, address pcend)
{
    char buf[256], tabletop[128];
    char* bufp;
    const address pc0 = pc;

    charout ('R');

    /* 予めバッファの先頭に .dc.l を作成しておく */
    bufp = strend (strcpy (buf, PSEUDO DC_L));
    if (option_U)
	strupr (buf);

    /* -Lxxxxxx も作成しておく */
    tabletop[0] = '-';
    make_proper_symbol (&tabletop[1], pc0);

    while (pc < pcend) {
	int dif = (int) peekl (pc + Ofst);
	char* p;

	if ((LONG) (pc0 + dif) < (LONG) BeginTEXT) {
	    make_proper_symbol (bufp, BeginTEXT);
	    p = strend (bufp);
	    *p++ = '-';
	    p = itox6d (p, BeginTEXT - (pc0 + dif));
	} else if ((LONG) (pc0 + dif) > (LONG) Last) {
	    make_proper_symbol (bufp, Last);
	    p = strend (bufp);
	    *p++ = '+';
	    p = itox6d (p, (pc0 + dif) - Last);
	} else {
	    make_proper_symbol (bufp, pc0 + dif);
	    p = strend (bufp);
	}

	strcpy (p, tabletop);

	/* 相対テーブルも -x でコメントを出力する */
	if (option_x)
	    byteout_for_xoption (pc, sizeof (LONG), buf);

	outputa (buf);
	newline (pc);

	pc += sizeof (LONG);
    }
}


/*

  ロングワードテーブルの出力

  input :
    pc : longword table begin address
    pcend : longword table end address

*/
private void
zgen (address pc, address pcend)
{
    address store;
    address limit = pcend + Ofst - sizeof (ULONG);

    charout ('z');

    for (store = pc + Ofst; store <= limit; store += sizeof (ULONG)) {
	char work[128];
	address label = (address) peekl (store);
	char* p;

	if ((LONG) label < (LONG) BeginTEXT) {
	    make_proper_symbol (work, BeginTEXT);
	    p = strend (work);
	    *p++ = '-';
	    itox6d (p, (ULONG) (BeginTEXT - label));
	} else if ((LONG) label > (LONG) Last) {
	    make_proper_symbol (work, Last);
	    p = strend (work);
	    *p++ = '+';
	    itox6d (p, (ULONG) (label - Last));
	} else
	    make_proper_symbol (work, label);

	output_opecode (PSEUDO DC_L);
	outputa (work);
	newline (store - Ofst);
    }

    if (store - Ofst != pcend)
	dataout (store - Ofst, pcend + Ofst - store, UNKNOWN);
}


/*

  ユーザー定義のテーブルの出力

  input :
    pc : user defined table begin address
    pcend : user defined table end address( not used )
    mode : table attribute( not used )
  return :
    table end address

*/
static boolean	end_by_break;

private address
tablegen (address pc, address pcend, lblmode mode)
{
    int loop, i;
    table* tableptr;

    DEBUG_PRINT (("enter tablegen\n"));
    charout ('t');

    if ((tableptr = search_table (pc)) == NULL)
	err ("Table が無い %06x (!?)\n" , pc);

    end_by_break = FALSE;
    pc = tableptr->tabletop;

    for (loop = 0; loop < tableptr->loop; loop++) {	/* implicit loop */
	for (i = 0; i < tableptr->lines; i++) {		/* line# loop */
	    DEBUG_PRINT (("loop=%d i=%d\n", loop, i));
	    pc = tablegen_sub (&tableptr->formulaptr[i],
				tableptr->tabletop, pc, i + 1 < tableptr->lines);
#if 0
	    if ((lptr = search_label (Eval_PC)) != NULL) {
		DEBUG_PRINT (("THERE IS LABEL (%x)\n", Eval_PC));
		label_line_out (Eval_PC, FALSE);
	    }
#endif
	    if (end_by_break)
		goto ret;
	}
    }

ret:
    DEBUG_PRINT (("exit tablegen\n"));
    return pc;
}


/*

  ユーザ定義のテーブルの出力

  input :
    exprptr : ユーザ定義のテーブル１行の構造体
    tabletop : table top address
    pc : pointing address
    notlast : true iff this line is not last line at table
  return :
    next pointing address

*/

private address
tablegen_sub (formula* exprptr, address tabletop, address pc, int notlast)
{

    ParseMode = PARSE_GENERATING;
    Eval_TableTop = tabletop;
    Eval_Count = 0;
    Eval_PC = pc;

    do {
	extern int	yyparse (void);

	lblbuf*	lptr = next (Eval_PC + 1);
	address	nextlabel = lptr->label;
	int	str_length;
	boolean	pc_inc_inst;

	Eval_ResultString[0] = '\0';
	Lexptr = exprptr->expr;

	DEBUG_PRINT (("parsing %s\n", Lexptr));
	yyparse ();

	/* CRID, BREAKID, EVENID(.even 出力時)以外は全て TRUE */
	pc_inc_inst = TRUE;

	switch (exprptr->id) {
	case LONGSIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_L, 4);
	    break;
	case WORDSIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_W, 2);
	    break;
	case BYTESIZE:
	    output_opecode (PSEUDO DC_B);
	    outputa (Eval_ResultString);
	    newline (Eval_PC);
	    Eval_PC++;
	    break;

	case SINGLESIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_S, 4);
	    break;
	case DOUBLESIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_D, 8);
	    break;
	case EXTENDSIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_X, 12);
	    break;
	case PACKEDSIZE:
	    tablegen_dc (nextlabel, lptr, PSEUDO DC_P, 12);
	    break;

#if 0
	case LONGID:
	    if (nextlabel < Eval_PC + Eval_Bytes*4)
		tablegen_label (Eval_PC, Eval_PC + Eval_Bytes * 4, lptr);
	    else
		datagen (Eval_PC, Eval_PC + Eval_Bytes * 4, LONGSIZE);
	    Eval_PC += Eval_Bytes * 4;
	    break;

	case WORDID:
	    if (nextlabel < Eval_PC + Eval_Bytes * 2)
		tablegen_label (Eval_PC, Eval_PC + Eval_Bytes * 2, lptr);
	    else
		datagen (Eval_PC, Eval_PC + Eval_Bytes * 2, WORDSIZE);
	    Eval_PC += Eval_Bytes * 2;
	    break;
#endif
	case BYTEID:
	    if (nextlabel < Eval_PC + Eval_Bytes)
		tablegen_label (Eval_PC, Eval_PC + Eval_Bytes, lptr);
	    else
		datagen (Eval_PC, Eval_PC + Eval_Bytes, BYTESIZE);
	    Eval_PC += Eval_Bytes;
	    break;
	case ASCIIID:
	    if (nextlabel < Eval_PC + Eval_Bytes)
		tablegen_label (Eval_PC, Eval_PC + Eval_Bytes, lptr);
	    else
		strgen (Eval_PC, Eval_PC + Eval_Bytes);
	    Eval_PC += Eval_Bytes;
	    break;
	case ASCIIZID:
	    str_length = strlen ((char*)Eval_PC + Ofst) + 1;
	    if (nextlabel < Eval_PC + str_length)
		tablegen_label (Eval_PC, Eval_PC + str_length, lptr);
	    else
		strgen (Eval_PC, Eval_PC + str_length);
	    Eval_PC += str_length;
	    break;
	case LASCIIID:
	    str_length = *(unsigned char*) (Eval_PC + Ofst);
	    datagen (Eval_PC, Eval_PC + 1, BYTESIZE);
	    Eval_PC++;
	    if (nextlabel < Eval_PC + str_length)
		tablegen_label (Eval_PC, Eval_PC + str_length, lptr);
	    else
		strgen (Eval_PC, Eval_PC + str_length);
	    Eval_PC += str_length;
	    break;
	case EVENID:
	    output_opecode (PSEUDO EVEN);
	    newline (Eval_PC);
	    if ((int)Eval_PC & 1)
		Eval_PC++;	/* pc_inc_inst = TRUE; */
	    else
		pc_inc_inst = FALSE;
	    break;
	case CRID:
	    newline (Eval_PC);
	    pc_inc_inst = FALSE;
	    break;
	case BREAKID:
	    if (Eval_Break) {
		DEBUG_PRINT (("END_BY_BREAK!(%x)\n", (unsigned int) Eval_PC))
		end_by_break = TRUE;
		goto tableend;
	    }
	    pc_inc_inst = FALSE;
	    break;
	default:	/* reduce warning message */
	    break;
	}

	if ((lptr = search_label (Eval_PC)) != NULL) {
	    DEBUG_PRINT (("THERE IS LABEL sub (%x)%x\n",
		(unsigned int) Eval_PC, lptr->mode));

#if 0	/* does not work properly */
	    if (isTABLE (lptr->mode) && notlast)
		eprintf ("\nEncounter another table in table(%x).\n", Eval_PC);
#endif
	    if (isENDTABLE (lptr->mode)) {
		DEBUG_PRINT (("ENCOUNTER ENDTABLE(%x)", (unsigned int) Eval_PC));
		if (notlast && (exprptr+1)->id == EVENID) {
		    /* if .even here.... */
		    output_opecode (PSEUDO EVEN);
		    newline (Eval_PC);
		    Eval_PC += (int)Eval_PC & 1;
		}
		end_by_break = TRUE;
		goto tableend;
	    } else if (pc_inc_inst)
		/* labelout is not required at end of table */
		label_line_out (Eval_PC, FALSE);
	}
    } while (--Eval_Count > 0);

 tableend:
    DEBUG_PRINT (("exit tablegen_sub()\n"));
    return Eval_PC;
}

static void
tablegen_dc (address nextlabel, lblbuf* lptr, char* op, int size)
{
    if (nextlabel < Eval_PC + size)
	tablegen_label (Eval_PC, Eval_PC + size, lptr);
    else {
	output_opecode (op);
	outputa (Eval_ResultString);
	newline (Eval_PC);
    }
    Eval_PC += size;
}


/*

  テーブル中にラベルが存在した場合のデータブロック出力

  input :
    pc : point address
    pcend : point address' end address
    lptr : テーブル中のラベル

*/
private void
tablegen_label (address pc, address pcend, lblbuf* lptr)
{
    address nlabel = lptr->label;
    lblmode nmode = lptr->mode;

    DEBUG_PRINT (("tablegen_label(%x,%x,%x)\n", (unsigned int)pc,
			(unsigned int) pcend, (unsigned int) nlabel));

    pc = datagen (pc, nlabel, nmode & 0xff);
    while (pc < pcend) {
	lblmode mode = nmode;

	lptr = next (pc + 1);
	while (lptr->shift)
	    lptr = Next (lptr);

	nlabel = lptr->label;
	nmode = lptr->mode;

	label_line_out (pc, mode);
	pc = isPROLABEL (mode) ? textgen (pc, min (nlabel, pcend))
			       : datagen (pc, min (nlabel, pcend), mode & 0xff);
    }

    DEBUG_PRINT (("exit tablegen_label\n"));
}



/*

  -M オプションのコメント処理

  input :
    pc : address of 'cmpi.?, move.b, addi.b, subi.? #??' instruction
    buffer : pointer to output buffer

*/
#define IMM_B1 (*(unsigned char*) (store + 1))
#define IMM_W1 (*(unsigned char*) (store + 0))
#define IMM_W2 (*(unsigned char*) (store + 1))
#define IMM_L1 (*(unsigned char*) (store + 0))
#define IMM_L2 (*(unsigned char*) (store + 1))
#define IMM_L3 (*(unsigned char*) (store + 2))
#define IMM_L4 (*(unsigned char*) (store + 3))

private void
byteout_for_moption (address pc, char* buffer, int size)
{
    char* p;
    address store = pc + Ofst;

    /* moveq.l は即値の位置が違う */
    if (size == LONGSIZE && (*(char*) store) >= 0x70)
	size = BYTESIZE;
    else
	store += 2;

    /* 表示可能な文字か調べる */
    if (size == BYTESIZE) {
	if (!isprint (IMM_B1))
	    return;
    }
    else if (size == WORDSIZE) {
	/* 上位バイトは表示可能もしくは 0 */
	if (IMM_W1 && !isprint (IMM_W1))
	    return;

	/* 下位バイトは必ず表示可能 */
	if (!isprint (IMM_W2))
	    return;
    }
    else if (size == LONGSIZE) {
	/* 最上位バイトは表示可能もしくは 0 */
	if (IMM_L1 && !isprint (IMM_L1))
	    return;

	/* 真ん中の二バイトは必ず表示可能 */
	if (!isprint (IMM_L2) || !isprint (IMM_L3))
	    return;

	/* 最下位バイトは表示可能もしくは 0 */
	if (IMM_L4 && !isprint (IMM_L4))
	    return;
    }
    else {	/* pack, unpk */
#ifdef PACK_UNPK_LOOSE
	/* 上位バイトは表示可能もしくは 0 */
	if (IMM_W1 && !isprint (IMM_W1))
	    return;

	/* 下位バイトは表示可能もしくは 0 */
	if (IMM_W2 && !isprint (IMM_W2))
	    reutrn;

	/* ただし両方とも 0 ではいけない */
	if (IMM_W1 == 0 && IMM_W2 == 0)
	    return;
#else
	/* 二バイトとも必ず表示可能 */
	if (!isprint (IMM_W1) || !isprint (IMM_W2))
	    return;
#endif
    }

    p = tab_out (Mtab, buffer);
    *p++ = '\'';

    if (size == BYTESIZE) {
	*p++ = IMM_B1;
    }
    else if (size == WORDSIZE) {
	if (IMM_W1)
	    *p++ = IMM_W1;
	*p++ = IMM_W2;
    }
    else if (size == LONGSIZE) {
	if (IMM_L1)
	    *p++ = IMM_L1;
	*p++ = IMM_L2;
	*p++ = IMM_L3;
	if (IMM_L4)
	    *p++ = IMM_L4;
	else {
	    strcpy (p, "'<<8");
	    return;
	}
    }
    else {	/* pack, unpk */
#ifdef PACK_UNPK_LOOSE
	if (IMM_W1)
	    *p++ = IMM_W1;
	if (IMM_W2)
	    *p++ = IMM_W2;
	else {
	    strcpy (p, "'<<8");
	    return;
	}
#else
	*p++ = IMM_W1;
	*p++ = IMM_W2;
#endif
    }

    *p++ = '\'';
    *p++ = '\0';
}
#undef IMM_B1
#undef IMM_W
#undef IMM_W1
#undef IMM_W2
#undef IMM_L1
#undef IMM_L2
#undef IMM_L3
#undef IMM_L4



/*

  -x オプションのコメント処理

  input :
    pc : address of instruction
    byte : instruction length in byte
    buffer : pointer to output buffer

*/
extern void
byteout_for_xoption (address pc, ULONG byte, char* buffer)
{
    int i;
    char c;
    address store = pc + Ofst;
    char* p = tab_out (Xtab, buffer);

    for (c = '\0', i = 0; i < byte; i += 2, store += 2) {
	if (c)
	    *p++ = c;		/* 二回目からはカンマが必要 */
	*p++ = '$';
	p = itox4_without_0supress (p, peekw (store));
	c = ',';
    }
}




/*

  bss 領域の出力

  input :
    pc : data begin address
    pcend : data end address
    size : data size

*/
private void
bssgen (address pc, address nlabel, opesize size)
{
    ULONG byte;

    charout ('$');
    nlabel = (address) min ((ULONG) nlabel, (ULONG) Last);
    byte = nlabel - pc;

    if ((LONG)nlabel >= 0) {
	if (size == WORDSIZE && byte == 2)
	    output_opecode (PSEUDO DS_W "1");
	else if (size == LONGSIZE && byte == 4)
	    output_opecode (PSEUDO DS_L "1");
	else {
	    output_opecode (PSEUDO DS_B);
	    outputfa ("%d", byte);
	}
	newline (pc);
    }
}


/*

  ラベルの付け替え

  input :
    code : disassembled opecode
    operand : operand ( op1,op2,op3 or op4 )

  return :
    TRUE if output is 'label[+-]??' style ( used to avoid as.x bug )
    (注)as.xには対応しなくなった為、返値は削除.

	Ablong		PCDISP		IMMED	    PCIDX
    0	(label)		(label,pc)	#label	    (label,pc,ix)
    1	(label-$num)	(label-$num,pc)	#label-$num (label-$num,pc,ix)
    2	(label+$num)	(label+$num,pc)	#label+$num (label+$num,pc,ix)

(odはアドレス依存かつexodが4の時だけlabel化する)
	PCIDXB		    PCPOSTIDX		    PCPREIDX
	(label,pc,ix)	    ([label,pc],ix,label)   ([label,pc,ix],label)

(bd,odはアドレス依存かつexbd,exodがそれぞれ4の時だけlabel化)
	AregIDXB	    AregPOSTIDX		    AregPREIDX
	(label,an,ix)	    ([label,an],ix,label)   ([label,an,ix],label)

*/
private void
labelchange (disasm* code, operand* op)
{
    address arg1;
    LONG shift;

    if (op->opval != (address)-1 &&
	(op->labelchange1 ||
	 ((op->ea == AbLong || ((op->ea == IMMED) && (code->size2 == LONGSIZE)))
	   && INPROG (op->opval, op->eaadrs))
	)
       ) {
	char buff[64];
	char* base = buff;
	char* ext_operand = NULL;

	switch (op->ea) {
	default:
	case IMMED:
	    break;

	case AbLong:
	    ext_operand = ")";
	    break;

	case AregPOSTIDX:
	case AregPREIDX:
	    if (!INPROG (op->opval, op->eaadrs))
		goto bd_skip;
	    ext_operand = strpbrk (op->operand, "],");
	    break;

	case PCDISP:
	    if ((char)op->labelchange1 < 0)
		break;		/* bsr label 等は括弧なし */
	    /* fall through */
	case AregIDXB:
	case PCIDX:
	    ext_operand = strchr (op->operand, ',');
	    break;
	case PCIDXB:
	    ext_operand = strchr (op->operand, ',');
	    goto check_size;
	case PCPOSTIDX:
	case PCPREIDX:
	    ext_operand = strpbrk (op->operand, "],");
check_size: if (ext_operand[-2] == (char)'.')
		ext_operand -= 2;	/* サイズ付きなら付け加える */
	    break;
	}

	if ((LONG)op->opval < (LONG)BeginTEXT) {
	    arg1 = BeginTEXT;
	    shift = (LONG)op->opval - (LONG)BeginTEXT;
	} else if ((ULONG)op->opval > (ULONG)Last) {
	    arg1 = Last;
	    shift = (ULONG)op->opval - (ULONG)Last;
	} else {
	    lblbuf* label_ptr = search_label (op->opval);

	    if (!label_ptr)
		/* ここで opval より手前のラベルを探して */
		/* ±shift 形式にするべきか??		 */
		return;
	    shift = label_ptr->shift;
	    arg1 = op->opval - shift;
	}

	switch (op->ea) {
	default:
	    break;
	case IMMED:
	    *base++ = '#';
	    break;
	case PCDISP:
	    if ((char)op->labelchange1 < 0)
		break;		/* bsr label 等は括弧なし */
	    /* fall through */
	case AbLong:
	case PCIDX:
	case AregIDXB:
	case PCIDXB:
	    *base++ = '(';
	    break;
	case AregPOSTIDX:
	case AregPREIDX:
	case PCPOSTIDX:
	case PCPREIDX:
	    *base++ = '(';
	    *base++ = '[';
	    break;
	}

	base = make_symbol (base, arg1, shift);

	if (ext_operand)
	    strcpy (base, ext_operand);

	strcpy (op->operand, buff);
    }

bd_skip:


/* アウタディスプレースメント用 */
    if (op->labelchange2 && op->opval2 != (address)-1
	&& INPROG (op->opval2, op->eaadrs2))
    {
	char* ptr;

	if ((LONG)op->opval2 < (LONG)BeginTEXT) {
	    arg1 = BeginTEXT;
	    shift = (LONG)op->opval2 - (LONG)BeginTEXT;
	} else if ((ULONG)op->opval2 > (ULONG)Last) {
	    arg1 = Last;
	    shift = (ULONG)op->opval2 - (ULONG)Last;
	} else {
	    lblbuf* label_ptr = search_label (op->opval2);

	    if (!label_ptr)
		return;
	    shift = label_ptr->shift;
	    arg1 = op->opval2 - shift;
	}

	ptr = make_symbol (strrchr (op->operand, ',') + 1, arg1, shift);
	*ptr++ = ')';
	*ptr++ = '\0';
    }
}


/* ラベル定義行のコロン出力 */
static INLINE void
add_colon (char* ptr, int xdef)
{
    if (SymbolColonNum) {
	*ptr++ = ':';
	/* -C3 又は、-C2 かつ外部定義なら :: */
	if (SymbolColonNum > 2 || (SymbolColonNum == 2 && xdef))
	    *ptr++ = ':';
    }
    *ptr = '\0';
}

/*

  ラベル定義行出力

  input :
    adrs : label address
    mode : label mode

  シンボル名が定義されていれば「シンボル名::」、未定義なら「Lxxxxxx:」
  などの形式で出力する(コロンの数やラベル先頭文字はオプションの状態で変わる).
  シンボル属性によっては全く出力されないこともある.

*/
private void
label_line_out (address adrs, lblmode mode)
{
    char  buf[128];
    symbol*  symbolptr;

    if (isHIDDEN (mode))
	return;

    if (Exist_symbol && (symbolptr = symbol_search (adrs))) {
	symlist* sym = &symbolptr->first;
	do {
	    if (sym->type == SectionType || (sym->type == 0)) {
		add_colon (strend (strcpy (buf, sym->sym)), sym->type);
		outputa (buf);
		newline (adrs);
	    }
	    sym = sym->next;
	} while (sym);
	return;
    }

    /* シンボル名が無ければ Lxxxxxx の形式で出力する */
    buf[0] = Label_first_char;
    add_colon (itox6_without_0supress (&buf[1], (ULONG)adrs), 0);
    outputa (buf);
    newline (adrs);
}

private void
label_line_out_last (address adrs, lblmode mode)
{
    char  buf[128];
    symbol*  symbolptr;

    if (isHIDDEN (mode))
	return;

    if (Exist_symbol && (symbolptr = symbol_search (adrs))) {
	symlist* sym = &symbolptr->first;
	do {
	    /* SectionType == 0 のシンボル名は出力しないこと.	*/
	    /* それは次のセクションの先頭で出力する.		*/
	    if (sym->type == SectionType) {
		add_colon (strend (strcpy (buf, sym->sym)), sym->type);
		outputa (buf);
		newline (adrs);
	    }
	    sym = sym->next;
	} while (sym);
	return;
    }

    /* ソースコード最後のラベルは絶対に出力する必要がある. */
    if (SectionType == 0)
	label_line_out (adrs, mode);

}



/*

  オペランドとしてのラベル出力

  input :
    adrs : label address
    mode : label mode

  .end及び.dc.l疑似命令のオペランドを作成する為に呼ばれる.
  コロンは付かない.
  +-shtが付く可能性がある.

*/
private void
label_op_out (address adrs)
{
    char buf[128];

    make_proper_symbol (buf, adrs);
    outputa (buf);
}


/*

  ラベルシンボルの生成

  input :
    buff : buffer of label symbol
    adrs : address

  最も近いシンボルを探し、同じ値がなければshift値を計算して
  make_symbol()を呼び出す.

*/
extern void
make_proper_symbol (char* buf, address adrs)
{
    address arg1;
    LONG shift;

    if ((LONG)adrs < (LONG)BeginTEXT) {		/* must be LONG, not ULONG */
	arg1 = BeginTEXT;
	shift = (LONG)adrs - (ULONG)BeginTEXT;
    } else if ((ULONG)adrs > (ULONG)Last) {
	arg1 = Last;
	shift = (ULONG)adrs - (ULONG)Last;
    } else {
	lblbuf* lblptr = search_label (adrs);

	if (lblptr && (shift = lblptr->shift) != 0)
	    arg1 = adrs - shift;
	else {
	    arg1 = adrs;
	    shift = 0;
	}
    }
    make_symbol (buf, arg1, shift);
}


/*

  シンボルを生成する

  input :
    ptr : symbol buffer
    adrs : address
    sft : shift count

  return :
    pointer to generated symbol' tail

  シンボルテーブルがあれば、adrs はシンボルに置換され、なければ L?????? 形式となる
  sft != 0 ならその後に +sft or -sft がつく

  ラベル定義行の生成にはlabel_line_out()を使うこと.

*/
extern char*
make_symbol (char* ptr, address adrs, LONG sft)
{
    symbol* symbol_ptr;

    if (Exist_symbol && (symbol_ptr = symbol_search (adrs)) != NULL) {
	strcpy (ptr, symbol_ptr->first.sym);
	ptr = strend (ptr);
    } else {
	*ptr++ = Label_first_char;
	ptr = itox6_without_0supress (ptr, (ULONG)adrs);
    }

    if (sft == 0)
	return ptr;

    if (sft > 0)
	*ptr++ = '+';
    else {
	*ptr++ = '-';
	sft = -sft;
    }
    return itox6d (ptr, sft);
}


private const char*
ctimez (time_t* clock)
{
    char* p = ctime (clock);
    char* n;

    if (p == NULL)
	return "(invalid time)";

    n = strchr (p, '\n');
    if (n)
	*n = '\0';
    return p;
}


static INLINE FILE*
open_header_file (void)
{
    FILE* fp = NULL;
    const char* fname = Header_filename ? : getenv ("dis_header");

    if (fname && *fname && (fp = fopen (fname, "rt")) == NULL) {
#if 0
	eprintf ("\n"
		 "ヘッダ記述ファイル \"%s\" がオープン出来ません.\n"
		 "標準の内容を出力します.\n", file);
#else
	eprintf ("\nヘッダ記述ファイルがオープン出来ません.\n");
#endif
    }

    return fp;
}


/*

  ヘッダを書く

  input :
    filename	: execute filename
    filedate	: execute file' time stamp
    argv0	: same to argv[0] given to main()
    comline	: commandline

*/
private void
makeheader (char* filename, time_t filedate, char* argv0, char* comline)
{
    char  buffer[256];
    char* envptr;
    char  cc = CommentChar;

    static char	op_include[]	= PSEUDO INCLUDE;
    static char	op_cpu[]	= PSEUDO CPU;
    static char	op_equ[]	= PSEUDO EQU;
    static char	op_xdef[]	= PSEUDO XDEF;

    if (option_U) {
	strupr (op_include);
	strupr (op_cpu);
	strupr (op_equ);
	strupr (op_xdef);
    }

    outputf ("%c=============================================" CR, cc);
    outputf ("%c  Filename %s" CR, cc, filename);
    outputf ("%c  Time Stamp %s" CR, cc, ctimez (&filedate));
    outputf ("%c" CR, cc);
#ifndef OSKDIS
    outputf ("%c  Base address %06x" CR, cc, Head.base);
    outputf ("%c  Exec address %06x" CR, cc, Head.exec);
    outputf ("%c  Text size    %06x byte(s)" CR, cc, Head.text);
    outputf ("%c  Data size    %06x byte(s)" CR, cc, Head.data);
    outputf ("%c  Bss  size    %06x byte(s)" CR, cc, Head.bss);
#endif	/* !OSKDIS */
    outputf ("%c  %d Labels" CR, cc, get_Labelnum ());
    {
	time_t	t = time (NULL);
	outputf ("%c  Code Generate date %s" CR, cc, ctimez (&t));
    }
    if ((envptr = getenv (DIS_ENVNAME)) != NULL)
	outputf ("%c  Environment %s" CR, cc, envptr);

    outputf ("%c  Commandline %s %s" CR, cc, argv0, comline);
    outputf ("%c          DIS version %s" CR, cc, Version);

#ifdef	OSKDIS
    outputf ("%c********************************************" CR, cc);

    outputf ("%c  Revision    %04x"	CR, cc, HeadOSK.sysrev);
    outputf ("%c  Module Size %d bytes" CR, cc, HeadOSK.size);
    outputf ("%c  Owner       %d.%d"	CR, cc, HeadOSK.owner >> 16,
						HeadOSK.owner & 0xffff);
    outputf ("%c  Name Offset %08x" CR, cc, HeadOSK.name);
    outputf ("%c  Permit      %04x" CR, cc, HeadOSK.accs);
    outputf ("%c  Type/Lang   %04x" CR, cc, HeadOSK.type);
    outputf ("%c  Attribute   %04x" CR, cc, HeadOSK.attr);
    outputf ("%c  Edition     %d"   CR, cc, HeadOSK.edition);
    outputf ("%c  Entry       %08x" CR, cc, HeadOSK.exec);
    outputf ("%c  Excpt       %08x" CR, cc, HeadOSK.excpt);

    if ((HeadOSK.type & 0x0F00) == 0x0e00 ||	/* Driver   */
	(HeadOSK.type & 0x0F00) == 0x0b00 ||	/* Trap     */
	(HeadOSK.type & 0x0F00) == 0x0100) {	/* Program  */
	outputf ("%c  Memory Size %d" CR, cc, HeadOSK.mem);
    }
    if ((HeadOSK.type & 0x0F00) == 0x0100 ||	/* Program  */
	(HeadOSK.type & 0x0F00) == 0x0b00) {	/* Trap     */
	outputf ("%c  Stack Size  %d"   CR, cc, HeadOSK.stack);
	outputf ("%c  M$IData     %08x" CR, cc, HeadOSK.idata);
	outputf ("%c  M$IRefs     %08x" CR, cc, HeadOSK.irefs);
    }
    if ((HeadOSK.type & 0x0F00) == 0x0b00) {	/* Trap     */
	outputf ("%c  M$Init      %08x" CR, cc, HeadOSK.init);
	outputf ("%c  M$Term      %08x" CR, cc, HeadOSK.term);
    }

    outputf ("%c********************************************" CR
	     "%c" CR, cc, cc);
    strcpy (buffer, Top - (ULONG)Head.base + HeadOSK.name);	/* モジュール名 */
    switch (HeadOSK.type & 0x0f00) {
	case 0x0100:	/* Program */
	case 0x0b00:	/* Trap    */
	    outputf ("\tpsect\t%s,$%04x,$%04x,%d,%d,L%06x",
			buffer, HeadOSK.type, HeadOSK.attr,
			HeadOSK.edition, HeadOSK.stack, HeadOSK.exec);
	    if (HeadOSK.excpt)
		outputf (",L%06x", HeadOSK.excpt);
	    break;
	case 0x0200:	/* Subroutine */
	case 0x0c00:	/* System     */
	case 0x0d00:	/* FileMan    */
	case 0x0e00:	/* Driver     */
	    outputf ("\tpsect\t%s,$%04x,$%04x,%d,%d,L%06x",
			buffer, HeadOSK.type, HeadOSK.attr,
			HeadOSK.edition, 0, HeadOSK.exec);
	    if (HeadOSK.excpt)
		outputf (",L%06x", HeadOSK.excpt);
	    break;
    }
#else
    outputf ("%c=============================================" CR CR, cc);

    /* dis_header の出力 */
    {
	FILE* fp = open_header_file ();

	if (fp) {
	    while (fgets (buffer, sizeof buffer, fp)) {
		char* p = strchr (buffer, '\n');
		if (p)
		    *p = '\0';
		outputf ("%s" CR, buffer);
	    }
	    fclose (fp);
	}
	else {
	    if (Doscall_mac_path)
		outputf ("%s%s" CR, op_include, Doscall_mac_path);
	    if (Iocscall_mac_path)
		outputf ("%s%s" CR, op_include, Iocscall_mac_path);
	    if (Fefunc_mac_path)
		outputf ("%s%s" CR, op_include, Fefunc_mac_path);
	    if (Sxcall_mac_path)
		outputf ("%s%s" CR, op_include, Sxcall_mac_path);
	}
	outputf (CR);
    }
#endif	/* OSKDIS */

#ifndef	OSKDIS
    {	/* 外部定義シンボルの出力 */
	char* colon = (SymbolColonNum == 0) ? ""
		    : (SymbolColonNum == 1) ? ":" : "::";
	if (output_symbol_table (op_equ, op_xdef, colon))
	    outputf (CR);
    }
#endif	/* !OSKDIS */

    /* .cpu 疑似命令の出力 */
    outputf ("%s%s" CR, op_cpu, mputype_numstr (Current_Mputype));

}


private char*
mputype_numstr (mputypes m)
{
    if (m & M000) return "68000";
    if (m & M010) return "68010";
    if (m & M020) return "68020";
    if (m & M030) return "68030";
    if (m & M040) return "68040";
 /* if (m & M060) */
		  return "68060";
}


/*

  オペコードの出力

  input :
    ptr : buffer of opecode ( like "move.l",0 )

*/
private void
output_opecode (char* ptr)
{
    char buf[32];

    if (option_U)
	ptr = strupr (strcpy (buf, ptr));
    outputa (ptr);
}



/************************************************/
/*	    LIBC/XC 以外で使用する関数		*/
/************************************************/

#ifdef NEED_MAKE_CMDLINE
private char*
make_cmdline (char** argv)
{
    char* ptr = Malloc (1);

    for (*ptr = '\0'; *argv; argv++) {
	ptr = Realloc (ptr, strlen (ptr) + strlen (*argv) + 2);
	strcat (strcat (ptr, " "), *argv);
    }

    return ptr;
}
#endif


/************************************************/
/*		以下は OSKDIS 用の関数		*/
/************************************************/

#ifdef	OSKDIS

private void
oskdis_gen (lblbuf* lptr)
{
    lblbuf* lptr = gen (CR, BeginBSS, next (BeginTEXT), XDEF_BSS, 0, -1);
    lblmode nmode = lptr->mode;
    address pc = lptr->label;

    output_opecode (CR "\tvsect" CR CR);
    while (pc <= BeginDATA) {
	lblmode  mode = nmode;
	address  nlabel;

	lptr   = Next (lptr);
	nlabel = lptr->label;
	nmode  = lptr->mode;

	if (pc == BeginDATA) break; /* 泥縄 */

	vlabelout (pc, mode);
	bssgen (pc, nlabel, mode & 0xff);
	pc = nlabel;
    }

    DEBUG_PRINT (("\nTEST=%08x, DATA=%08x, BSS=%08x, LAST=%08x\n",
			BeginTEXT, BeginDATA, BeginBSS, Last));

    lptr = next (BeginDATA);	/* vsect の初期化データ領域をポイント */
    switch (HeadOSK.type & 0x0f00) {
	case 0x0100:		/* Program  */
	case 0x0b00:		/* Trap     */
	    {
		ULONG* idatsiz = Top - (ULONG) Head.base + (ULONG) HeadOSK.idata + 4;
		lptr = idatagen (BeginDATA + *idatsiz, lptr);
	    }			/* vsect の初期化データを出力 */
	    break;
	default:
	    lptr = idatagen (Last, lptr);
	    break;
    }
    pc = lptr->label;
    DEBUG_PRINT (("pc=0x%.8x, last=0x%.8x\n", pc, Last));

    output_opecode (CR "\tends" CR);	/* end vsect */

    if (pc < Last) {
	output_opecode (CR "\tvsect\tremote" CR CR);
	while (pc <= Last) {
	    lblmode  mode = nmode;
	    address  nlabel;

	    lptr   = Next (lptr);
	    nlabel = lptr->label;
	    nmode  = lptr->mode;

	    if (pc == Last) break;	/* 泥縄 */

	    vlabelout (pc, mode);
	    bssgen (pc, nlabel, mode & 0xff);
	    pc = nlabel;
	}
	output_opecode (CR "\tends" CR);	/* end vsect remote */
    }

    output_opecode (CR "\tends" CR);	/* end psect */
    output_opecode (CR "\tend" CR);
}

/*

　初期化データを出力する(下請け)

*/
private address
idatagen_sub (address pc, address wkpc, address pcend, lblmode mode)
{

    while (pc < pcend) {
	ULONG byte = pcend - pc;

	switch (mode & 0xff) {
	case LONGSIZE:
	    if (byte != 4)
		goto XX;
	    if (mode & CODEPTR) {
		ULONG x = peekl (wkpc);
		char* s = (x == 0) ?		PSEUDO DC_L "_btext\t* $%08x" CR
			: (x == HeadOSK.name) ? PSEUDO DC_L "_bname\t* $%08x" CR
					      : PSEUDO DC_L "L%06x" CR;
		outputf (s, x);
	    } else if (mode & DATAPTR)
		outputf (PSEUDO DC_L "L%06x" CR, peekl (wkpc) + BeginBSS);
	    else
		outputf (PSEUDO DC_L "$%08x" CR, peekl (wkpc));
	    break;

	case WORDSIZE:
	    if (byte != 2)
		goto XX;
	    outputf (PSEUDO DC_W "$%04x" CR, peekw (wkpc));
	    break;

	default:
XX:	    byteout ((ULONG)wkpc - (ULONG)Top + (ULONG)Head.base, byte, FALSE);
	}
	pc += byte;
	wkpc += byte;
    }
    return wkpc;
}

/*

  初期化データを出力する

  input:
    end     : block end address
    lptr    : lblbuf* ( contains block start address )

*/
private lblbuf*
idatagen (address end, lblbuf* lptr)
{
    address pc = lptr->label;
    address wkpc = Top - (ULONG) Head.base + (ULONG) HeadOSK.idata + 8;

    newline (pc);

    while (pc < end) {
	lblmode mode = lptr->mode;

	do {
	    lptr = Next (lptr);
	} while (lptr->shift);

	label_line_out (pc, mode);

	DEBUG_PRINT (("PC=%08x, NEXT=%08x, MODE=%08x\n", pc, lptr->label, mode));

	wkpc = idatagen_sub (pc, wkpc, lptr->label, mode);
	pc = nlabel;
    }

    return lptr;
}


/*

  vsect ラベル出力

  input :
    adrs : label address
    mode : label mode

*/
private void
vlabelout (address adrs, lblmode mode)
{
    char  buff[128];
    int   opt_c_save = option_C;

    if (isHIDDEN (mode))
	return;
    
    if (opt_c_save == 0)
	option_C = 1;	/* 非初期化 vsect はラベルだけの記述は出来ない */
    label_line_out (adrs, mode);
    option_C = opt_c_save;
    outputa (buff);
}

/*

  ワードテーブルの出力

  input:
    pc : word table begin address
    pcend : word table end address

*/
private void
wgen (address pc, address pcend)
{
    char buf[128];
    address store;

    charout ('w');

    for (store = pc + Ofst; store + 2 <= pcend + Ofst; store += 2) {
	address label = (int)*(signed short*)store;

	if ((LONG)label < (LONG)BeginTEXT) {
	    make_proper_symbol (buf, BeginTEXT);
	    strcat (buf, "-");
	    itox6d (buf + strlen (buf), (ULONG)(BeginTEXT - label));
	} else if ((LONG)label > (LONG)Last) {
	    make_proper_symbol (work, Last);
	    strcat (buf, "+");
	    itox6d (buf + strlen (buf), (ULONG)(label - Last));
	} else
	    make_proper_symbol (work, label);

	output_opecode (PSEUDO DC_W);
	outputa (buf);
	newline (store - Ofst);
    }

    if (store != pcend + Ofst)
	dataout (store - Ofst, pcend + Ofst - store, UNKNOWN);
}

#endif	/* OSKDIS */


/* EOF */
