/* $Id: disasm.c,v 2.76 1995/01/07 11:33:16 ryo Exp $
 *
 *	ソースコードジェネレータ
 *	逆アセンブルモジュール
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <string.h>

#include "disasm.h"
#include "estruct.h"
#include "etc.h"	/* peek[wl] */
#include "fpconv.h"
#include "global.h"
#include "hex.h"

#ifdef	OSKDIS
#include "label.h"
#endif	/* OSKDIS */


#define BYTE1	    (*(unsigned char*) ptr)
#define BYTE2	    (*(unsigned char*) (ptr + 1))
#define BYTE3	    (*(unsigned char*) (ptr + 2))
#define BYTE4	    (*(unsigned char*) (ptr + 3))
#define BYTE5	    (*(unsigned char*) (ptr + 4))
#define BYTE6	    (*(unsigned char*) (ptr + 5))
#define WORD1	    (peekw (ptr))
#define WORD2	    (peekw (ptr + 2))
#define WORD3	    (peekw (ptr + 4))
#define LONG05	    (peekl (ptr + 2))
#define SignBYTE2   (*(signed char *) (ptr + 1))
#define SignBYTE4   (*(signed char *) (ptr + 3))
#define SignWORD2   ((WORD) peekw (ptr + 2))
#define SignWORD3   ((WORD) peekw (ptr + 4))
#define SignLONG05  ((LONG) peekl (ptr + 2))

/*  アドレッシングモード (bitmaped)  */
/*
    Dn ----------------------------------------------------+
    An ---------------------------------------------------+|
    (An) ------------------------------------------------+||
    (An)+ ----------------------------------------------+|||
    -(An) ---------------------------------------------+||||
    (d16,An) -----------------------------------------+|||||
    (d8,An,ix) --------------------------------------+||||||
    ??? --------------------------------------------+|||||||
    (abs).w ---------------------------------------+||||||||
    (abs).l --------------------------------------+|||||||||
    (d16,pc) ------------------------------------+||||||||||
    (d8,pc,ix) ---------------------------------+|||||||||||
    #imm --------------------------------------+||||||||||||

					       |||||||||||||	*/
#define DATAREG	    0x00001	/*	-------------------@	*/
#define ADRREG	    0x00002	/*	------------------@-	*/
#define ADRIND	    0x00004	/*	-----------------@--	*/
#define POSTINC	    0x00008	/*	----------------@---	*/
#define PREDEC	    0x00010	/*	---------------@----	*/
#define DISPAREG    0x00020	/*	--------------@-----	*/
#define IDXAREG	    0x00040	/*	-------------@------	*/
#define ABSW	    0x00100	/*	-----------@--------	*/
#define ABSL	    0x00200	/*	----------@---------	*/
#define DISPPC	    0x00400	/*	---------@----------	*/
#define IDXPC	    0x00800	/*	--------@-----------	*/
#define IMMEDIATE   0x01000	/*	-------@------------	*/
#define SRCCR	    0x10000	/*	---@----------------	*/
#define MOVEOPT	    0x80000	/*	@-------------------	*/

#define CHANGE	    0x0037f	/*	----------@@-@@@@@@@	*/
#define CTRLCHG	    0x00364	/*	----------@@-@@--@--	*/
#define CONTROL	    0x00f64	/*	--------@@@@-@@--@--	*/
#define MEMORY	    0x01f7c	/*	-------@@@@@-@@@@@--	*/
#define DATA	    0x01f7d	/*	-------@@@@@-@@@@@-@	*/
#define ALL	    0x01f7f	/*	-------@@@@@-@@@@@@@	*/


#ifdef	OSKDIS
#define	DC_WORD "dc"
#else
#define	DC_WORD ".dc"
#endif

/* private 関数プロトタイプ */
private void	    op00 (address, disasm*);
private void	    op01 (address, disasm*);
private void	    op02 (address, disasm*);
private void	    op03 (address, disasm*);
private void	    op04 (address, disasm*);
private void	    op05 (address, disasm*);
private void	    op06 (address, disasm*);
private void	    op07 (address, disasm*);
private void	    op08 (address, disasm*);
private void	    op09 (address, disasm*);
private void	    op0a (address, disasm*);
private void	    op0b (address, disasm*);
private void	    op0c (address, disasm*);
private void	    op0d (address, disasm*);
private void	    op0e (address, disasm*);
private void	    op0f (address, disasm*);
private void	    setBitField (operand* op, address ptr);
private void	    setMMUfc (disasm* code, operand* op, int fc);
private void	    moveope   (address, disasm*);
private void	    addsubope (address, disasm*, const char*);
private void	    packope   (address, disasm*, const char*);
private void	    bcdope    (address, disasm*, const char*);
private void	    addsize (char*, opesize);
private void	    setreglist (char*, address);
private void	    setIMD (disasm*, operand*, address, opesize);
private void	    setAnDisp (disasm*, operand*, int, int);
private void	    setEA (disasm*, operand*, address, int);


#ifndef OSKDIS
char**	OSlabel;
char**	FElabel;
char**	SXlabel;			/* char* [SXCALL_MAX]へのポインタ */
char	OSCallName[MAX_MACRO_LEN] = "DOS";
char	FECallName[MAX_MACRO_LEN] = "FPACK";
char	SXCallName[MAX_MACRO_LEN] = "SXCALL";
#endif	/* OSKDIS */


/* 奇数の添え字は意味なし. コプロID*2 = 添え字(比較の時のシフトをなくすため) */
char	FPUID_table[16];

/* バイト命令の不定バイトのチェック */
boolean Disasm_Exact = TRUE;

/* 逆アセンブルされた文字列を返すかどうか */
boolean Disasm_String = TRUE;

/* 未使用の Aline , Fline TRAP を未定義とするか */
boolean Disasm_UnusedTrapUndefined = TRUE;

/* Address Error の起こる命令を未定義とするか */
boolean Disasm_AddressErrorUndefined = TRUE;

/* dbf を dbra として出力するか */
boolean Disasm_Dbra = TRUE;

/* ニーモニックを省略して出力 ( cmpi -> cmp etc. ) */
boolean Disasm_MnemonicAbbreviation = FALSE;

/* ! Disasm_UnusedTrapUndefined 時に SX-Window を有効に */
boolean Disasm_SX_Window = FALSE;

/* CPU32 命令セット対応(デフォルトで対応しない) */
boolean	Disasm_CPU32;

#ifdef	COLDFIRE
/* ColdFire 命令セット対応(デフォルトで対応しない) */
boolean	Disasm_ColdFire;
#endif

/* 命令セット(デフォルトで 68000) */
mputypes MPU_types = M000;

/* MMU 命令セット(デフォルトで未対応) */
short	MMU_type;

/* FPU 命令セット(デフォルトで未対応) */
short	FPCP_type;

/* 未使用フィールドの検査(デフォルトで最も厳密に検査) */
int	UndefRegLevel = 0x0f;

static address	PC;
       address	PCEND;


#define SX_WINDOW_EXIT	0xa352

#define DOS_EXIT	0x00
#define DOS_EXIT2	0x4c
#define DOS_KEEPPR	0x31
#ifdef	DOS_KILL_PR_IS_RTSOP
#define DOS_KILL_PR	0xf9
#endif


static const int pow2[16] = {
    1, 2, 4, 8, 16, 32, 64, 128,
    256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
};


#define IfNeedStr	if (Disasm_String)
#define SETSIZE()	(code->size = code->size2 = (WORD1 >> 6) & 3)
#define OPECODE(op)	do { if (Disasm_String) strcpy (code->opecode, op);} while (0)
#define UNDEFINED()	(code->flag = UNDEF)
#define REJECTBYTESIZE() \
    do { \
	if ((WORD1 & 0x38) == 0x8 && code->size == BYTESIZE) { \
	    UNDEFINED(); return; \
	} \
    } while (0)
#define FPCOND(code, byte) \
    do { \
	if (byte & 0x20) { \
	    UNDEFINED(); return; \
	} else \
	    setfpcond (code, byte); \
    } while (0)
#define FPOPESET \
    do { \
	if ((ExtensionFlags[WORD2 & 0x7f] & FPCP_type) == 0) { \
	    UNDEFINED(); return; \
	} \
	OPECODE (ExtensionField[ WORD2 & 0x7f ]); \
    } while (0)
#define MMUCOND(code, byte) \
    do { \
	if (byte & 0x30) { UNDEFINED(); return; } \
	setmmucond (code, byte); \
    } while (0)


/* 指定MPUだけが対象なら未定義命令 */
#define	REJECT(x) \
(void)({ \
    if ((MPU_types & ~(x)) == 0) { UNDEFINED(); return; } \
    code->mputypes &= ~(x); \
})

/* 68060でISP無しなら未定義命令 */
#define REJECT060noISP() \
(void)({ \
    if ((MPU_types & MISP) == 0) { \
	if ((MPU_types & ~M060) == 0) { UNDEFINED(); return; } \
	code->mputypes &= ~M060; \
    } \
})

/* 68040/68060でFPSP無しなら未定義命令(6888xなら常に有効) */
#define REJECTnoFPSP() \
(void)({ \
    if ((FPCP_type & (F4SP|F6SP)) == 0) { \
	if ((FPCP_type & ~(F040|F060)) == 0) { UNDEFINED(); return; } \
	code->mputypes &= ~(M040|M060); \
    } \
})

/* 68060でFPSP無しなら未定義命令(6888x/68040なら常に有効) */
#define REJECT060noFPSP() \
(void)({ \
    if ((FPCP_type & F6SP) == 0) { \
	if ((FPCP_type & ~F060) == 0) { UNDEFINED(); return; } \
	code->mputypes &= ~M060; \
    } \
})

/* 68851無しなら未定義命令 */
#define REJECTnoPMMU() \
(void)({ \
    if ((MMU_type & MMU851) == 0) { UNDEFINED(); return; } \
    code->mputypes = M020; \
})


/*

  逆アセンブルモジュール
  ptr に(格納)アドレスを与えると、code に逆アセンブル結果が入ってくる
  何バイト命令かを返す

  address *pcptr -> global in this file.

  */
extern int
dis (address ptr, disasm* code, address* pcptr)
{
    static void (*decode[]) (address, disasm*) = {
	op00, op01, op02, op03, op04, op05, op06, op07,
	op08, op09, op0a, op0b, op0c, op0d, op0e, op0f
    };

    code->opecode[0] =
    code->op1.operand[0] =
    code->op2.operand[0] =
    code->op3.operand[0] =
    code->op4.operand[0] = '\0';

    code->op1.ea = code->op2.ea = code->op3.ea = code->op4.ea = (adrmode) -1;

    code->op1.opval   = code->op2.opval   = code->op3.opval   = code->op4.opval   =
    code->op1.opval2  = code->op2.opval2  = code->op3.opval2  = code->op4.opval2  =
    code->op1.eaadrs  = code->op2.eaadrs  = code->op3.eaadrs  = code->op4.eaadrs  =
    code->op1.eaadrs2 = code->op2.eaadrs2 = code->op3.eaadrs2 = code->op4.eaadrs2 =
	(address) -1;

    code->op1.exbd = code->op2.exbd = code->op3.exod = code->op4.exod =
    code->op1.exod = code->op2.exod = code->op3.exod = code->op4.exod = -1;

    code->op1.labelchange1 = code->op2.labelchange1 =
    code->op3.labelchange1 = code->op4.labelchange1 =
    code->op1.labelchange2 = code->op2.labelchange2 =	/* od用 */
    code->op3.labelchange2 = code->op4.labelchange2 =	/* od用 */
	FALSE;

#ifndef __HUMAN68K__	/* Safe and Normal coding */
    code->size2 = code->size = NOTHING;
    code->default_size = WORDSIZE;
    code->bytes = 2;
    code->flag = OTHER;
    code->mputypes = ~0;
    code->fpuid = -1;
    code->opflags = 0;
#else	/* Fast but very DANGEROUS coding. */
    {
	long* p = (long*) &code->size;

	*(opesize*) p++ = (opesize) NOTHING;	/* size */
	*(opesize*) p++ = (opesize) NOTHING;	/* size2 */
	*(opesize*) p++ = (opesize) WORDSIZE;	/* default_size */
	*(int*) p++	= (int) 2;		/* bytes */
	*(opetype*) p++ = (opetype) OTHER;	/* flag */
	*p		= (~0 << 24)		/* mputypes */
			+ (0xff << 16)		/* fpuid */
			+ (0 << 8) + 0;		/* opflags */
    }
#endif

    PC = *pcptr;

    (decode[BYTE1 >> 4]) (ptr, code);

    if ((code->bytes > 2) && (PCEND < (PC + code->bytes))) {

	  code->op1.opval   = code->op2.opval   = code->op3.opval   = code->op4.opval
	= code->op1.opval2  = code->op2.opval2  = code->op3.opval2  = code->op4.opval2
	= code->op1.eaadrs  = code->op2.eaadrs  = code->op3.eaadrs  = code->op4.eaadrs
	= code->op1.eaadrs2 = code->op2.eaadrs2 = code->op3.eaadrs2 = code->op4.eaadrs2
	= (address)-1;

	code->flag = UNDEF;
    }

    if (code->flag == UNDEF) {
	IfNeedStr {
	    code->mputypes = ~0;
	    code->fpuid = -1;

	    code->op1.ea = code->op2.ea = code->op3.ea = code->op4.ea = (adrmode) -1;

	    code->op1.labelchange1 = code->op2.labelchange1 =
	    code->op3.labelchange1 = code->op4.labelchange1 =
	    code->op1.labelchange2 = code->op2.labelchange2 =
	    code->op3.labelchange2 = code->op4.labelchange2 = FALSE;

	    code->op2.operand[0] =
	    code->op3.operand[0] =
	    code->op4.operand[0] = '\0';

	    strcpy (code->opecode, DC_WORD);
	    itox4d (code->op1.operand, WORD1);
	}
	code->bytes = 2;
	code->size = code->size2 = code->default_size = WORDSIZE;
    }
    *pcptr += code->bytes;	/* PC += code->bytes */

    return code->bytes;
}


/*  inline functions here... */

INLINE static void
setDn (operand* op, int regno)
{
    op->ea = DregD;
    IfNeedStr {
	op->operand[0] = 'd';
	op->operand[1] = (regno & 7) + '0';
	op->operand[2] = 0;
    }
}

INLINE static void
setAn (operand* op, int regno)
{
    op->ea = AregD;
    IfNeedStr {
	op->operand[0] = 'a';
	op->operand[1] = (regno & 7) + '0';
	op->operand[2] = 0;
    }
}


INLINE static void
setRn (operand* op, int regno)
{
    op->ea = (regno & 8) ? AregD : DregD;
    IfNeedStr {
	op->operand[0] = (regno & 8) ? 'a' : 'd';
	op->operand[1] = (regno & 7) + '0';
	op->operand[2] = '\0';
    }
}


INLINE static void
setFPn (operand* op, int regno)
{
    op->ea = FPregD;
    IfNeedStr {
	char *p = op->operand;

	*p++ = 'f';
	*p++ = 'p';
	*p++ = (regno & 7) + '0';
	*p++ = '\0';
    }
}


INLINE static adrmode
setFPCRSRlist (operand* op, int regno)
{
    static char* fpreglist[8] = {
	NULL,
	"fpiar",
	"fpsr",
	"fpsr/fpiar",
	"fpcr",
	"fpcr/fpiar",
	"fpcr/fpsr",
	"fpcr/fpsr/fpiar"
    };
    static adrmode adr[8] = {
	0,
	ALL,
	DATA,
	MEMORY,
	DATA,
	MEMORY,
	MEMORY,
	MEMORY
    };

    op->ea = FPCRSR;
    IfNeedStr {
	strcpy (op->operand, fpreglist[regno & 7]);
    }
    return adr[regno & 7];

}


/*  レジスタリスト( fmovem ) */
private void
setFPreglist (char* operandstr, address ptr)
{
    unsigned int field;
    boolean flag, already;
    int i, start;

    if (!Disasm_String)
	return;

    if ((WORD1 & 0x38) == 0x20)		/* pre-decrement ?   movem とは逆!! */
	field = BYTE4;
    else {
	int i;
	for (field = 0, i = 0; i < 8; i++)
	    field |= (WORD2 & pow2[i] ? pow2[7 - i] : 0);
    }

    start = 0;
    for (i = 0, flag = FALSE, already = FALSE; i < 9; i++) {
	if (!flag && field & pow2[i]) {
	    start = i;
	    flag = TRUE;
	} else {
	    if (flag && (field & pow2[i]) == 0) {
		char* p = strend (operandstr);

		if (already)
		    *p++ = '/';
		*p++ = 'f';
		*p++ = 'p';
		*p++ = start + '0';
		if (start != i - 1) {
		    *p++ = '-';
		    *p++ = 'f';
		    *p++ = 'p';
		    *p++ = (i - 1) + '0';
		}
		*p = '\0';
		already = TRUE;
		flag = FALSE;
	    }
	}
    }
}



/*  and or 用 ( eor はアドレッシングモードが微妙に異なる )  */
INLINE static void
logicope (address ptr, disasm* code)
{
    SETSIZE ();
    if (BYTE1 & 1) {
	setDn (&code->op1, BYTE1 >> 1);
	setEA (code, &code->op2, ptr, MEMORY & CHANGE);
    } else {
	setEA (code, &code->op1, ptr, DATA);
	setDn (&code->op2, BYTE1 >> 1);
    }
}


/*  mul div 用  */
INLINE static void
muldivope (address ptr, disasm* code)
{
    IfNeedStr {
	strcat (code->opecode, (BYTE1 & 1) ? "s" : "u");
    }
    code->size = code->size2 = WORDSIZE;
    setEA (code, &code->op1, ptr, DATA);
    setDn (&code->op2, BYTE1 >> 1);
}

/* データレジスタのペア */
static INLINE void
setPairDn (operand* op, int reg1, int reg2)
{
    op->ea = RegPairD;
    IfNeedStr {
	strcpy (op->operand, "d0:d0");
	op->operand[1] += reg1;
	op->operand[4] += reg2;
    }
}

/*  ロングフォーム mul div 用  */
static INLINE void
longmuldivope (address ptr, disasm* code)
{
    code->size = code->size2 = LONGSIZE;
    code->bytes = 4;
    setEA (code, &code->op1, ptr, DATA);
    setPairDn (&code->op2, WORD2 & 7, (BYTE3 >> 4) & 7);
}


/*  cc 条件用  */
/*  IfNeedStr をパスした状態で呼び出すこと */
INLINE static void
setcond (address ptr, disasm* code)
{
    static const char cond[16][4] = {
	"t" , "f" , "hi", "ls", "cc", "cs", "ne", "eq",
	"vc", "vs", "pl", "mi", "ge", "lt", "gt", "le"
    };

    strcat (code->opecode, cond[BYTE1 & 0xf]);
}


/*  コプロ cc条件用 */
INLINE static void
setfpcond (disasm* code, unsigned char predicate)
{
    static const char cond[32][5] = {
	 "f"	, "eq"	, "ogt" , "oge" , "olt" , "ole" , "ogl" , "or"	,
	 "un"	, "ueq" , "ugt" , "uge" , "ult" , "ule" , "ne"	, "t"	,
	 "sf"	, "seq" , "gt"	, "ge"	, "lt"	, "le"	, "gl"	, "gle" ,
	 "ngle" , "ngl" , "nle" , "nlt" , "nge" , "ngt" , "sne" , "st"
    };

    strcat (code->opecode, cond[predicate & 0x1f]);
}


/*  68851 cc条件用  */
INLINE static void
setmmucond (disasm* code, unsigned char predicate)
{
    static const char cond[16][4] = {
	 "bs", "bc", "ls", "lc", "ss", "sc", "as", "ac",
	 "ws", "wc", "is", "ic", "gs", "gc", "cs", "cc"
    };

    strcat (code->opecode, cond[predicate & 0x0f]);
}



/*  #0 - #7 用  */
INLINE static void
set07 (operand* op, int num)
{
    op->ea = IMMED;
    IfNeedStr {
	op->operand[0] = '#';
	op->operand[1] = (num & 7) + '0';
	op->operand[2] = '\0';
    }
}


/*  addq , subq , bit操作用 #1 ～ #8  */
INLINE static void
set18 (operand* op, int num)
{
    op->ea = IMMED;
    IfNeedStr {
	num &= 7;
	op->operand[0] = '#';
	op->operand[1] = num ? (num + '0') : '8';
	op->operand[2] = 0;
    }
}

/*  相対分岐用  */
INLINE static void
setrelative (char* optr, int disp, address* opval)
{
    *opval = PC + 2 + (WORD) disp;
    IfNeedStr {
	itox8d (optr, (LONG) *opval);
    }
}


/*  相対分岐用(4byteコード(コプロ)用)  */
INLINE static void
setrelative4 (char* optr, int disp, address* opval)
{
    *opval = PC + 4 + (WORD) disp;
    IfNeedStr {
	itox8d (optr, (LONG) *opval);
    }
}


/* ロングワード版 */
INLINE static void
setlongrelative (char* optr, int disp, address* opval)
{
    *opval = PC + 2 + disp;
    IfNeedStr {
	itox8d (optr, (LONG) *opval);
    }
}



/*  functions to decode machine instruction op00 - op0f */

private void
op00 (address ptr, disasm* code)
{

    /* movep */
    if ((WORD1 & 0x0138) == 0x0108) {
	char* p;

	REJECT060noISP ();
	OPECODE ("movep");
	code->size = code->size2 = (WORD1 & 0x40) ? LONGSIZE : WORDSIZE;
	if (WORD1 & 0x80) {
	    setDn (&code->op1, BYTE1 >> 1);
	    setAnDisp (code, &code->op2, WORD1 & 7, WORD2);
	    p = strstr (code->op2.operand, ".w");
	} else {
	    setAnDisp (code, &code->op1, WORD1 & 7, WORD2);
	    setDn (&code->op2, BYTE1 >> 1);
	    p = strstr (code->op1.operand, ".w");
	}
	if (p)				/* movep (0,an) が (an) に最適化  */
	    strcpy (p, p + 2);		/* される事はないので ".w" は不要 */
	return;
    }

    /* btst、bchg、bclr、bset */
    {
	static const char opcode01[4][5] = {
	    "btst", "bchg", "bclr", "bset"
	};

	/* ビット位置が immediate */
	if ((BYTE1 & 0x0f) == 8) {
	    if (BYTE3) {
		UNDEFINED(); return;
	    }
	    OPECODE (opcode01[BYTE2 >> 6]);
	    code->size = code->default_size = (WORD1 & 0x38) ? BYTESIZE : LONGSIZE;
	    code->size2 = UNKNOWN;
	    setIMD (code, &code->op1, ptr, BYTESIZE);
	    setEA (code, &code->op2, ptr, DATA ^ IMMEDIATE);
	    return;
	}

	/* ビット位置が Dn */
	if (BYTE1 & 1) {
	    OPECODE (opcode01[BYTE2 >> 6]);
	    code->size = code->size2 = code->default_size =
					(WORD1 & 0x38) ? BYTESIZE : LONGSIZE;
	    setDn (&code->op1, BYTE1 >> 1);		/* btst 以外は可変 */
	    setEA (code, &code->op2, ptr, (WORD1 & 0xc0) ? DATA & CHANGE : DATA);
	    return;
	}

	/* どちらでもなければ他の命令 */
    }

    /* ori、andi、subi、addi、eori、cmpi */
    if (BYTE1 != 0x0e && BYTE2 < 0xc0) {
	SETSIZE ();
	setIMD (code, &code->op1, ptr, code->size);

	if (BYTE1 == 0 || BYTE1 == 2 || BYTE1 == 0xa) {
	    /* ori, andi, eori */
	    setEA (code, &code->op2, ptr, SRCCR | (DATA & CHANGE));
	    /* to ccr は byte サイズ */
	    if (code->op2.ea == SRCCR)
		code->default_size = code->size;
	}
	else if (BYTE1 == 0x0c && MPU_types & ~(M000|M010)) {
	    /* cmpi */
	    if ((WORD1 & 0x3e) == 0x3a)
		code->mputypes &= ~(M000|M010);
	    setEA (code, &code->op2, ptr, DATA ^ IMMEDIATE);
	}
	else
	    setEA (code, &code->op2, ptr, DATA & CHANGE);

	IfNeedStr {
	    static const char opcode00[7][5] = {
		"ori", "andi", "subi", "addi", "", "eori", "cmpi"
	    };

	    strcpy (code->opecode, opcode00[BYTE1 >> 1]);

	    /* cmpi.* 及び addi.b、subi.b にはコメントを付ける */
	    if (BYTE1 == 0x0c || (code->size == BYTESIZE
					&& (BYTE1 == 0x06 || BYTE1 == 0x04)))
		code->opflags += FLAG_NEED_COMMENT;

	    if (Disasm_MnemonicAbbreviation
		/* eori か to Dn 以外なら "i" は省略可能 */
		&& (BYTE1 == 10 || code->op2.ea != DregD)) {
		    if (code->opecode[3])
			code->opecode[3] = '\0';	/* xxxi */
		    else
			code->opecode[2] = '\0';	/* xxi */

		/* addi/subi で 1 <= imm <= 8 なら  */
		/* imm にサイズを付ける(最適化対策) */
		if ((BYTE1 == 4 || BYTE1 == 6) && ((ULONG)code->op1.opval - 1) <= 7)
		    addsize (code->op1.operand, code->size);
	    }
	}
	return;
    }

    /* cmp2、chk2 */
    if ((WORD1 & 0x09c0) == 0x00c0 && BYTE1 != 6
     && (WORD2 & 0x07ff) == 0x0000) {

	REJECT (M000|M010);
	REJECT060noISP ();
	OPECODE ((WORD2 & 0x0800) ? "chk2" : "cmp2");
	code->size = code->size2 = (BYTE1 >> 1) & 0x03;
	code->bytes = 4;
	setEA (code, &code->op1, ptr, CONTROL);
	setRn (&code->op2, BYTE3 >> 4);
	return;
    }

    /* cas */
    if ((WORD1 & 0x09c0) == 0x08c0 && (BYTE1 & 0x06) != 0
     && (WORD2 & 0xfe38) == 0x0000) {
	REJECT (M000|M010);

	IfNeedStr {
	    strcpy (code->opecode, "cas");
	    strcpy (code->op1.operand, "d0");
	    strcpy (code->op2.operand, "d0");
	    code->op1.operand[1] +=  WORD2	 & 7;
	    code->op2.operand[1] += (WORD2 >> 6) & 7;
	}
	code->bytes = 4;
	code->op1.ea = code->op2.ea = DregD;
	setEA (code, &code->op3, ptr, CHANGE ^ DATAREG ^ ADRREG);
	code->size = code->size2 = ((BYTE1 >> 1) & 3) - 1;	/* サイズ値が普通と違う */
	return;
    }

    /* cas2 */
    if ((WORD1 & 0x0dff) == 0x0cfc
     && ((WORD2 | WORD3) & 0x0e38) == 0x0000) {
	REJECT (M000|M010);
	REJECT060noISP ();

	IfNeedStr {
	    strcpy (code->opecode, "cas2");
	    strcpy (code->op3.operand, "(r0):(r0)");
	    code->op3.operand[1]  = ((BYTE3 >> 4) & 8) ? 'a' : 'd';
	    code->op3.operand[2] += ((BYTE3 >> 4) & 7);
	    code->op3.operand[6]  = ((BYTE5 >> 4) & 8) ? 'a' : 'd';
	    code->op3.operand[7] += ((BYTE5 >> 4) & 7);
	}
	setPairDn (&code->op1,  WORD2	    & 7,  WORD3       & 7);
	setPairDn (&code->op2, (WORD2 >> 6) & 7, (WORD3 >> 6) & 7);
	code->op3.ea = RegPairID;
	code->bytes = 6;
	code->size = code->size2 = (BYTE1 & 0x02) ? LONGSIZE : WORDSIZE;
	return;
    }

    if ((WORD1 & 0x0f00) == 0x0e00 && (WORD2 & 0x07ff) == 0x0000) {
	REJECT (M000);

	OPECODE ("moves");
	SETSIZE ();
	code->bytes = 4;
	if (BYTE3 & 0x08) {
	    setRn (&code->op1, BYTE3 >> 4);
	    code->op1.ea = ((BYTE3 >> 4) & 8) ? AregD : DregD;
	    setEA (code, &code->op2, ptr, CHANGE ^ DATAREG ^ ADRREG);
	}
	else {
	    setRn (&code->op2, BYTE3 >> 4);
	    code->op2.ea = ((BYTE3 >> 4) & 8) ? AregD : DregD;
	    setEA (code, &code->op1, ptr, CHANGE ^ DATAREG ^ ADRREG);
	}
	return;
    }

    /* 68020 の callm、rtm */
    if ((WORD1 & 0x0fc0) == 0x06c0) {
	REJECT (~M020);

	if ((WORD1 & 0x0030) == 0) {
	    OPECODE ("rtm");
	    setRn (&code->op1, WORD1);
	}
	else {
	    if (BYTE3) {
		UNDEFINED(); return;
	    }
	    IfNeedStr {
		strcpy (code->opecode, "callm");
		code->op1.operand[0] = '#';
		itox2d (code->op1.operand + 1, BYTE4);
	    }
	    code->op1.ea = IMMED;
	    code->op1.opval = (address)(ULONG) BYTE4;
	    code->bytes += 2;
	    setEA (code, &code->op2, ptr, CONTROL);
	}
	return;
    }

    UNDEFINED(); return;
}


private void
op01 (address ptr, disasm* code)
{
    code->size = code->size2 = BYTESIZE;
    moveope (ptr, code);
}


private void
op02 (address ptr, disasm* code)
{
    code->size = code->size2 = LONGSIZE;
    moveope (ptr, code);
}


private void
op03 (address ptr, disasm* code)
{
    code->size = code->size2 = WORDSIZE;
    moveope (ptr, code);
}


private void
moveope (address ptr, disasm *code)
{
    REJECTBYTESIZE ();

    setEA (code, &code->op1, ptr, ALL);
    if (code->flag == UNDEF)
	return;

    /* movea */
    if ((WORD1 & 0x1c0) == 0x40) {
	if (code->size == BYTESIZE) {
	    UNDEFINED(); return;
	}

	setAn (&code->op2, BYTE1 >> 1);
	IfNeedStr {
	    strcpy (code->opecode, "movea");
	    if (Disasm_MnemonicAbbreviation)
		code->opecode[4] = '\0';
	    /* movea.* #imm にはコメントを付ける */
	    if (code->op1.ea == IMMED)
		code->opflags += FLAG_NEED_COMMENT;
	}
	return;
    }

    setEA (code, &code->op2, ptr, (DATA & CHANGE) | MOVEOPT);

    IfNeedStr {
	strcpy (code->opecode, "move");

	/* move.* #imm にはコメントを付ける */
	if (code->op1.ea == IMMED) {
	    code->opflags += FLAG_NEED_COMMENT;

	    /* move.l #imm,Dn が moveq.l #imm,Dn に化けないように... */
	    if ((WORD1 & 0x31ff) == 0x203c		/* move.l #imm,Dn か? */
	     && (LONG05 < 0x80 || 0xffffff80 <= LONG05)	/* imm が範囲内か?    */
	    )
		strcat (code->op1.operand, ".l");
	}
    }
}


private void
op04 (address ptr, disasm* code)
{
    if ((WORD1 & 0xffc0) == 0x4c00) {
	switch (WORD2 & 0x8ff8) {
	case 0x0000:
	    REJECT (M000|M010);

	    if ((UndefRegLevel & 1)	/* 未定義フィールドが 0 or Dl と同じなら正常 */
	     && (WORD2 & 7) != 0 && (WORD2 & 7) != ((BYTE3 >> 4) & 7)) {
		UNDEFINED(); return;
	    }

	    OPECODE ("mulu");
	    code->bytes = 4;
	    code->size = code->size2 = LONGSIZE;
	    setEA (code, &code->op1, ptr, DATA);
	    setDn (&code->op2, BYTE3 >> 4);
	    return;
	case 0x0400:
	    REJECT (M000|M010);
	    REJECT060noISP ();
	    OPECODE ("mulu");
	    longmuldivope (ptr, code);
	    return;
	case 0x0800:
	    REJECT (M000|M010);

	    if ((UndefRegLevel & 1)	/* 未定義フィールドが 0 or Dl と同じなら正常 */
	     && (WORD2 & 7) != 0 && (WORD2 & 7) != ((BYTE3 >> 4) & 7)) {
		UNDEFINED(); return;
	    }

	    OPECODE ("muls");
	    code->bytes = 4;
	    code->size = code->size2 = LONGSIZE;
	    setEA (code, &code->op1, ptr, DATA);
	    setDn (&code->op2, BYTE3 >> 4);
	    return;
	case 0x0c00:
	    REJECT (M000|M010);
	    REJECT060noISP ();
	    OPECODE ("muls");
	    longmuldivope (ptr, code);
	    return;
	default:
	    break;
	}
    }

    if ((WORD1 & 0xffc0) == 0x4c40) {
	switch (WORD2 & 0x8cf8) {
	case 0x0000:
	    REJECT (M000|M010);

	    if ((BYTE3 >> 4) == BYTE4) {
		OPECODE ("divu");
		code->bytes = 4;
		code->size = code->size2 = LONGSIZE;
		setEA (code, &code->op1, ptr, DATA);
		setDn (&code->op2, WORD2);
		return;
	    } else {
		OPECODE ("divul");
		longmuldivope (ptr, code);
		return;
	    }
	case 0x0400:
	    REJECT (M000|M010);
	    REJECT060noISP ();
	    OPECODE ("divu");
	    longmuldivope (ptr, code);
	    return;
	case 0x0800:
	    REJECT (M000|M010);

	    if ((BYTE3 >> 4) == BYTE4) {
		OPECODE ("divs");
		code->bytes = 4;
		code->size = code->size2 = LONGSIZE;
		setEA (code, &code->op1, ptr, DATA);
		setDn (&code->op2, WORD2);
		return;
	    } else {
		OPECODE ("divsl");
		longmuldivope (ptr, code);
		return;
	    }
	case 0x0c00:
	    REJECT (M000|M010);
	    REJECT060noISP ();
	    OPECODE ("divs");
	    longmuldivope (ptr, code);
	    return;
	default:
	    break;
	}
    }

    if ((WORD1 & 0x0ff8) == 0x9c0) {
	REJECT (M000|M010);

	OPECODE ("extb");
	code->size = code->size2 = LONGSIZE;
	setDn (&code->op1, WORD1);
	return;
    }

    if ((WORD1 & 0x1c0) == 0x1c0) {
	OPECODE ("lea");
	code->size = code->default_size = LONGSIZE;
	code->size2 = UNKNOWN;
	setEA (code, &code->op1, ptr, CONTROL);
	setAn (&code->op2, BYTE1 >> 1);
	return;
    }

    if ((WORD1 & 0x140) == 0x100) {
	OPECODE ("chk");
	if ((WORD1 & 0x80) == 0x80)
	    code->size = code->size2 = WORDSIZE;
	else if ((WORD1 & 0x80) == 0x00) {
	    REJECT (M000|M010);
	    code->size = code->size2 = LONGSIZE;
	} else {
	    UNDEFINED(); return;
	}
	setEA (code, &code->op1, ptr, DATA);
	setDn (&code->op2, BYTE1 >> 1);
	return;
    }

    if (BYTE1 == 0x4e) {
	if (0x70 <= BYTE2 && BYTE2 <= 0x77) {
	    static const char opecode[8][6] = {
		"reset", "nop", "stop", "rte", "rtd", "rts", "trapv", "rtr"
	    };

	    if (BYTE2 == 0x74)
		REJECT (M000);			/* rtd は 68010 以降専用 */

	    OPECODE (opecode[BYTE2 & 0x0f]);
	    if (BYTE2 == 0x72 || BYTE2 == 0x74)
		setIMD (code, &code->op1, ptr, WORDSIZE);
	    if (0x73 <= BYTE2 && BYTE2 != 0x76) {
		code->flag = RTSOP;
		code->opflags += FLAG_NEED_NULSTR;
	    }
	    return;
	}
	if ((WORD1 & 0xf0) == 0x40) {
#ifdef	OSKDIS
	    if (BYTE2 & 15) {
		IfNeedStr {
		    strcpy (code->opecode, "TCALL");
		    itox2d (code->op1.operand, BYTE2 & 15);
		    itox4d (code->op2.operand, WORD2);
	        }
		code->op1.ea = IMMED;
		code->op2.ea = IMMED;
		code->op1.opval = (address) (BYTE2 & 15);
		code->op2.opval = WORD2;
	    }
	    else {
		IfNeedStr {
		    strcpy (code->opecode, "OS9");
		    itox4d (code->op1.operand, WORD2);
		    code->op1.ea = IMMED;
		    code->op1.opval = WORD2;
		}
		if (WORD2 == 0x0006	/* F$Exit  */
		 || WORD2 == 0x001e	/* F$RTE   */
		 || WORD2 == 0x002d) {	/* F$NProc */
		    code->flag = RTSOP;
		code->opflags += FLAG_NEED_NULSTR;
		}
	    }
	    code->bytes = 4;
	    return;
#else
	    IfNeedStr {
		strcpy (code->opecode, "trap");
		code->op1.operand[0] = '#';
		itod2 (code->op1.operand + 1, WORD1 & 15);
	    }
	    code->op1.ea = IMMED;
	    code->op1.opval = (address) (WORD1 & 15);
	    return;
#endif	/* OSKDIS */
	}

	if ((WORD1 & 0xf8) == 0x50) {	/* ワードリンク */
	    code->size = WORDSIZE;
	    setAn (&code->op1, WORD1);
#if 0
	    setIMD (code, &code->op2, ptr, WORDSIZE);
#endif
	    code->op2.ea = IMMED;
	    code->bytes += 2;
	    IfNeedStr {
		WORD d16 = WORD2;
		char* p = code->op2.operand;

		*p++ = '#';
		if (d16 < 0) {
		    *p++ = '-';
		    d16 = -d16 & 0xffff;
		}
		itox4d (p, d16);
		strcpy (code->opecode, "link");
	    }
	    return;
	}
	if ((WORD1 & 0xf8) == 0x58) {
	    OPECODE ("unlk");
	    setAn (&code->op1, WORD1);
	    return;
	}

	if ((WORD1 & 0xc0) == 0xc0) {
	    setEA (code, &code->op1, ptr, CONTROL);
	    if (code->flag == UNDEF)
		return;
	    IfNeedStr {
		strcpy (code->opecode, "jmp");
		code->opflags += FLAG_NEED_NULSTR;
	    }
	    code->jmp = code->op1.opval;
	    code->jmpea = code->op1.ea;
	    code->flag = JMPOP;
	    return;
	}

	if ((WORD1 & 0xc0) == 0x80) {
	    OPECODE ("jsr");
	    setEA (code, &code->op1, ptr, CONTROL);
	    if (code->flag == UNDEF)
		return;
	    code->jmp = code->op1.opval;
	    code->jmpea = code->op1.ea;
	    code->flag = JSROP;
	    return;
	}

	/* move to/from usp */
	if ((WORD1 & 0xf0) == 0x60) {
	    OPECODE ("move");
	    code->size = code->size2 = code->default_size = LONGSIZE;
	    if (WORD1 & 8) {
		IfNeedStr {
		    strcpy (code->op1.operand, "usp");
		}
		setAn (&code->op2, WORD1);
	    } else {
		setAn (&code->op1, WORD1);
		IfNeedStr {
		    strcpy (code->op2.operand, "usp");
		}
	    }
	    return;
	}

	if ((WORD1 & 0xfe) == 0x7a) {
	    static const struct {
		mputypes mpu;
		char name[7];
	    } creg_table[2][9] = { {
		{ M010|M020|M030|M040|M060,	"sfc"	}, /* 0x000 */
		{ M010|M020|M030|M040|M060,	"dfc"	}, /* 0x001 */
		{      M020|M030|M040|M060,	"cacr"	}, /* 0x002 */
		{		 M040|M060,	"tc"	}, /* 0x003 */
		{		 M040|M060,	"itt0"	}, /* 0x004 */
		{		 M040|M060,	"itt1"	}, /* 0x005 */
		{		 M040|M060,	"dtt0"	}, /* 0x006 */
		{		 M040|M060,	"dtt1"	}, /* 0x007 */
		{		      M060,	"buscr"	}, /* 0x008 */
	      }, {
		{ M010|M020|M030|M040|M060,	"usp"	}, /* 0x800 */
		{ M010|M020|M030|M040|M060,	"vbr"	}, /* 0x801 */
		{      M020|M030,		"caar"	}, /* 0x802 */
		{      M020|M030|M040,		"msp"	}, /* 0x803 */
		{      M020|M030|M040,		"isp"	}, /* 0x804 */
		{		 M040,		"mmusr"	}, /* 0x805 */
		{		 M040|M060,	"urp"	}, /* 0x806 */
		{		 M040|M060,	"srp"	}, /* 0x807 */
		{		      M060,	"pcr"	}, /* 0x808 */
	    } };
	    int i = (WORD2 & 0x800) >> 11;
	    int j = (WORD2 & 0x7ff);
	    const char* p = creg_table[i][j].name;

	    if (j > 8) {
		UNDEFINED(); return;
	    }
	    REJECT (~creg_table[i][j].mpu);

	    OPECODE ("movec");
	    code->size = code->size2 = code->default_size = LONGSIZE;
	    code->bytes += 2;

	    if (WORD1 & 1) {
		IfNeedStr {
		    strcpy (code->op2.operand, p);
		}
		setRn (&code->op1, BYTE3 >> 4);
		code->op2.ea = CtrlReg;
	    } else {
		IfNeedStr {
		    strcpy (code->op1.operand, p);
		}
		setRn (&code->op2, BYTE3 >> 4);
		code->op1.ea = CtrlReg;
	    }
	    return;
	}
    }

    if ((WORD1 & 0xfb8) == 0x880) {
	OPECODE ("ext");
	code->size = code->size2 = (WORD1 & 0x40 ? LONGSIZE : WORDSIZE);
	setDn (&code->op1, WORD1);
	return;
    }

    if ((WORD1 & 0xb80) == 0x880) {
	if (WORD2 == 0) {		/* register field empty ? */
	    UNDEFINED(); return;
	}
	OPECODE ("movem");
	code->size = code->size2 = (WORD1 & 0x40 ? LONGSIZE : WORDSIZE);
	code->bytes = 4;
	if (BYTE1 & 4) {
	    setEA (code, &code->op1, ptr, CONTROL | POSTINC);
	    setreglist (code->op2.operand, ptr);
	} else {
	    setreglist (code->op1.operand, ptr);
	    setEA (code, &code->op2, ptr, CTRLCHG | PREDEC);
	}
	return;
    }

    switch (WORD1 & 0xff8) {
    case 0x840:
	OPECODE ("swap");
	code->size = code->size2 = WORDSIZE;
	setDn (&code->op1, WORD1);
	return;
    case 0x848:
	REJECT (M000);

	IfNeedStr {
	    strcpy (code->opecode, "bkpt");
	    code->op1.operand[0] = '#';
	    code->op1.operand[1] = (WORD1 & 7) + '0';
	    code->op1.operand[2] = '\0';
	}
	code->op1.ea = IMMED;
	code->op1.opval = (address) (WORD1 & 7);
	return;
    case 0x808:
	REJECT (M000|M010);

	code->size = LONGSIZE;			/* ロングワードリンク */
	setAn (&code->op1, WORD1);
	code->op2.ea = IMMED;
	code->bytes += 4;
	IfNeedStr {
	    LONG d32 = LONG05;
	    char* p = code->op2.operand;

	    *p++ = '#';
	    if (d32 < 0) {
		*p++ = '-';
		d32 = -d32;
	    }
	    itox8d (p, d32);
	    strcpy (code->opecode, "link");
	}
	return;
    }

    switch (WORD1 & 0xfc0) {
    case 0x4c0:
	code->size = code->size2 = WORDSIZE;		/* move to ccr */
	setEA (code, &code->op1, ptr, DATA);
	IfNeedStr {
	    strcpy (code->opecode, "move");
	    strcpy (code->op2.operand, "ccr");
	}
	return;
    case 0x6c0:
	code->size = code->size2 = WORDSIZE;		/* move to sr */
	setEA (code, &code->op1, ptr, DATA);
	IfNeedStr {
	    strcpy (code->opecode, "move");
	    strcpy (code->op2.operand, "sr");
	}
	return;
    case 0x0c0:
	code->size = code->size2 = WORDSIZE;		/* move from sr */
	IfNeedStr {
	    strcpy (code->opecode, "move");
	    strcpy (code->op1.operand, "sr");
	}
	setEA (code, &code->op2, ptr, DATA);
	return;
    case 0x2c0:
	REJECT (M000);

	code->size = code->size2 = WORDSIZE;		/* move from ccr */
	IfNeedStr {
	    strcpy (code->opecode, "move");
	    strcpy (code->op1.operand, "ccr");
	}
	setEA (code, &code->op2, ptr, DATA);
	return;
    case 0x840:
	OPECODE ("pea");
	code->size = code->default_size = LONGSIZE;
	code->size2 = UNKNOWN;
	setEA (code, &code->op1, ptr, CONTROL);
	return;
    case 0xac0:
	if (WORD1 == 0x4afc) {
	    OPECODE ("illegal");
	    return;
	}
	else if (WORD1 == 0x4afa && Disasm_CPU32) {
	    OPECODE ("bgnd");
	    return;
	}
	OPECODE ("tas");
	code->size = code->size2 = code->default_size = BYTESIZE;
	setEA (code, &code->op1, ptr, DATA & CHANGE);
	return;
    case 0x800:
	OPECODE ("nbcd");
	code->size = code->size2 = code->default_size = BYTESIZE;
	setEA (code, &code->op1, ptr, DATA & CHANGE);
	return;
    }

    if ((BYTE1 & 1) == 0) {

	/* tst */
	if (BYTE1 == 0x4a) {
	    OPECODE ("tst");
	    SETSIZE ();				/* size が 0b11 なら tas */
	    setEA (code, &code->op1, ptr,
			(MPU_types & ~(M000|M010)) ? ALL : DATA & CHANGE);
	    /* An 直接、PC 相対、即値は 68020 以降専用 */
	    if ((ALL ^ (DATA & CHANGE)) & pow2[code->op1.ea])
		code->mputypes &= ~(M000|M010);
	    return;
	}

	/* negx, clr, neg, not */
	if (BYTE1 <= 0x46) {
	    static const char opecode[4][5] = {
		"negx", "clr", "neg", "not"
	    };

	    OPECODE (opecode[(BYTE1 & 0x0e) >> 1]);
	    SETSIZE ();
	    setEA (code, &code->op1, ptr, DATA & CHANGE);
	    return;
	}
    }

    UNDEFINED(); return;
}


private void
op05 (address ptr, disasm* code)
{

    /* DBcc */
    if ((BYTE2 >> 3) == 0x19) {

	IfNeedStr {
	    if (BYTE1 == 0x51 && Disasm_Dbra)
		strcpy (code->opecode, "dbra");		/* dbf -> dbra */
	    else {
		strcpy (code->opecode, "db");
		setcond (ptr, code);
	    }
	}

	setDn (&code->op1, WORD1);
	setrelative (code->op2.operand, SignWORD2, &code->op2.opval);
	code->jmp = code->op2.opval;
	code->jmpea = code->op2.ea = PCDISP;
	code->op2.labelchange1 = -1;	/* TRUE */
#ifdef	OSKDIS				/* OS-9/680x0 のアセンブラ(r68)では DBcc */
	code->size = NOTHING;		/* にサイズを付けるとエラーになるため	 */
#else
	code->size = WORDSIZE;
#endif
	code->size2 = WORDSIZE;
	code->bytes = 4;
	code->flag = BCCOP;
	return;
    }

    /* 68020 以降の trapcc */
    if ((WORD1 & 0xf8) == 0xf8 && (0xfa <= BYTE2) && (BYTE2 <= 0xfc)) {
	REJECT (M000|M010);

	IfNeedStr {
	    strcpy (code->opecode, "trap");
	    setcond (ptr, code);
	}
#if 0
	code->default_size = NOTHING;
#endif
	if (BYTE2 == 0xfa)
	    setIMD (code, &code->op1, ptr, code->size = WORDSIZE);
	else if (BYTE2 == 0xfb)
	    setIMD (code, &code->op1, ptr, code->size = LONGSIZE);
	return;
    }

    /* Scc */
    if ((WORD1 & 0xc0) == 0xc0) {
	IfNeedStr {
	    code->opecode[0] = 's';
	    code->opecode[1] = '\0';
	    setcond (ptr, code);
	}
	code->size = code->size2 = code->default_size = BYTESIZE;
	setEA (code, &code->op1, ptr, DATA & CHANGE);
	return;
    }

    /* addq、subq */
    {
	REJECTBYTESIZE ();
	SETSIZE ();
	OPECODE ((BYTE1 & 1) ? "subq" : "addq");
	set18 (&code->op1, BYTE1 >> 1);
	setEA (code, &code->op2, ptr, CHANGE);
	return;
    }

    /* NOT REACHED */
}


private void
op06 (address ptr, disasm* code)
{
    switch (BYTE1 & 0x0f) {
    case 0:
	IfNeedStr {
	    strcpy (code->opecode, "bra");
	    code->opflags += FLAG_NEED_NULSTR;
	}
	code->flag = JMPOP;
	break;
    case 1:
	OPECODE ("bsr");
	code->flag = JSROP;
	break;
    default:
	IfNeedStr {
	    code->opecode[0] = 'b';
	    code->opecode[1] = '\0';
	    setcond (ptr, code);
	}
	code->flag = BCCOP;
	break;
    }
    if (BYTE2 == 0x00) {
	code->bytes = 4;
	code->size = code->size2 = WORDSIZE;
	setrelative (code->op1.operand, SignWORD2, &code->op1.opval);
    } else if (BYTE2 == 0xff && (MPU_types & ~(M000|M010))) {
	code->mputypes &= ~(M000|M010);
	code->bytes = 6;
	code->size = code->size2 = LONGSIZE;
	setlongrelative (code->op1.operand, SignLONG05, &code->op1.opval);
    } else {
	code->size = code->size2 = SHORTSIZE;
	setrelative (code->op1.operand, SignBYTE2, &code->op1.opval);
    }
    code->default_size = NOTHING;
    code->jmp = code->op1.opval;
    code->jmpea = code->op1.ea = PCDISP;
    code->op1.labelchange1 = -1;	/* TRUE */
}


private void
op07 (address ptr, disasm* code)
{
    if (BYTE1 & 1) {
	UNDEFINED(); return;
    }
    code->size = code->default_size = LONGSIZE;
    code->size2 = UNKNOWN;
    IfNeedStr {
	strcpy (code->opecode, "moveq");
	code->op1.operand[0] = '#';
	itox2d (code->op1.operand + 1, BYTE2);
	/* moveq も -M でコメントを付ける */
	code->opflags += FLAG_NEED_COMMENT;
    }
    code->op1.ea = IMMED;
    code->op1.opval = (address)(ULONG) BYTE2;
    setDn (&code->op2, BYTE1 >> 1);
}


private void
op08 (address ptr, disasm* code)
{
    if ((WORD1 & 0x1f0) == 0x100) {
	bcdope (ptr, code, "sbcd");
	return;
    }

    if ((WORD1 & 0x1f0) == 0x140) {
	packope (ptr, code, "pack");
	return;
    }
    if ((WORD1 & 0x1f0) == 0x180) {
	packope (ptr, code, "unpk");
	return;
    }

    if ((WORD1 & 0xc0) == 0xc0) {
	OPECODE ("div");
	muldivope (ptr, code);
	return;
    } else {
	OPECODE ("or");
	logicope (ptr, code);
	return;
    }

    /* NOT REACHED */
}

/*  pack, unpack 用 */
static void
packope (address ptr, disasm* code, const char* opname)
{

    REJECT (M000|M010);

    code->op1.ea = code->op2.ea = (WORD1 & 8) ? AregIDPD : DregD;
    IfNeedStr {
	strcpy (code->opecode, opname);
	/* pack、unpk にはコメントを付ける */
	code->opflags += FLAG_NEED_COMMENT;
	if (WORD1 & 8) {
	    strcpy (code->op1.operand, "-(a0)");
	    strcpy (code->op2.operand, "-(a0)");
	    code->op1.operand[3] += WORD1	 & 7;
	    code->op2.operand[3] += (BYTE1 >> 1) & 7;
	}
	else {
	    strcpy (code->op1.operand, "d0");
	    strcpy (code->op2.operand, "d0");
	    code->op1.operand[1] += WORD1	 & 7;
	    code->op2.operand[1] += (BYTE1 >> 1) & 7;
	}
    }
    setIMD (code, &code->op3, ptr, WORDSIZE);
}


private void
op09 (address ptr, disasm* code)
{
    addsubope (ptr, code, "sub");
}


private void
op0a (address ptr, disasm* code)
{

#ifndef	OSKDIS
    /* SXlable != NULL なら必ず Disasm_SX_Window == TRUE */
    if (SXlabel && SXlabel[WORD1 & 0xfff]) {
	IfNeedStr {
	    strcpy (code->opecode, SXCallName);
	    strcpy (code->op1.operand, SXlabel[WORD1 & 0xfff]);
	    code->opflags += FLAG_CANNOT_UPPER;
	}
	if (WORD1 == SX_WINDOW_EXIT) {
	    code->flag = RTSOP;
	    code->opflags += FLAG_NEED_NULSTR;
	}
	code->size = code->size2 = code->default_size = NOTHING;
	return;
    }
#endif	/* !OSKDIS */

    /* 未使用の A-line を未定義命令と見なさない */
    if (!Disasm_UnusedTrapUndefined) {
	IfNeedStr {
	    strcpy (code->opecode, DC_WORD);
	    itox4d (code->op1.operand, WORD1);
	}
	code->size = code->size2 = code->default_size = WORDSIZE;
	return;
    }

}


private void
op0b (address ptr, disasm* code)
{

    /* cmpa */
    if ((WORD1 & 0xc0) == 0xc0) {
	code->size = code->size2 = (BYTE1 & 1) ? LONGSIZE : WORDSIZE;
	setEA (code, &code->op1, ptr, ALL);
	setAn (&code->op2, BYTE1 >> 1);
	IfNeedStr {
	    strcpy (code->opecode, "cmpa");
	    /* cmpa.* #imm にはコメントを付ける */
	    if (code->op1.ea == IMMED)
		code->opflags += FLAG_NEED_COMMENT;
	}
	return;
    }

    /* cmpm */
    if ((WORD1 & 0x138) == 0x108) {
	SETSIZE ();
	IfNeedStr {
	    strcpy (code->opecode, "cmpm");
	    if (Disasm_MnemonicAbbreviation)
		code->opecode[3] = '\0';
	    strcpy (code->op1.operand, "(a0)+");
	    strcpy (code->op2.operand, "(a0)+");
	    code->op1.operand[2] += WORD1	 & 7;
	    code->op2.operand[2] += (BYTE1 >> 1) & 7;
	}
	code->op1.ea = code->op2.ea = AregIDPI;
	return;
    }

    /* cmp */
    if ((BYTE1 & 1) == 0) {
	REJECTBYTESIZE ();
	SETSIZE ();
	setEA (code, &code->op1, ptr, ALL);
	setDn (&code->op2, BYTE1 >> 1);
	IfNeedStr {
	    strcpy (code->opecode, "cmp");
	    /* cmp.* #imm にはコメントを付ける */
	    if (code->op1.ea == IMMED)
		code->opflags += FLAG_NEED_COMMENT;
	}
	return;
    }

    /* eor */
    if (BYTE1 & 1) {
	OPECODE ("eor");
	SETSIZE ();
	setDn (&code->op1, BYTE1 >> 1);
	setEA (code, &code->op2, ptr, DATA & CHANGE);
	return;
    }

    UNDEFINED(); return;
}


private void
op0c (address ptr, disasm* code)
{

    if ((WORD1 & 0x1f0) == 0x100) {
	bcdope (ptr, code, "abcd");
	return;
    }

    {
	int tmp = WORD1 & 0x1f8;

	if (tmp == 0x140 || tmp == 0x148 || tmp == 0x188) {
	    OPECODE ("exg");
	    code->size = code->size2 = code->default_size = LONGSIZE;
	    switch (tmp) {
	    case 0x140:
		setDn (&code->op1, BYTE1 >> 1);
		setDn (&code->op2, WORD1);
		break;
	    case 0x148:
		setAn (&code->op1, BYTE1 >> 1);
		setAn (&code->op2, WORD1);
		break;
	    case 0x188:
		setDn (&code->op1, BYTE1 >> 1);
		setAn (&code->op2, WORD1);
		break;
	    }
	    return;
	}
    }

    if ((WORD1 & 0xc0) == 0xc0) {
	OPECODE ("mul");
	muldivope (ptr, code);
	return;
    } else {
	OPECODE ("and");
	logicope (ptr, code);
	return;
    }

    /* NOT REACHED */
}


private void
op0d (address ptr, disasm* code)
{
    addsubope (ptr, code, "add");
}


private void
op0e (address ptr, disasm* code)
{

#define BF_ONLY 0
#define BF_DREG 1
#define DREG_BF 2
    if ((WORD1 & 0xf8c0) == 0xe8c0) {
	static const struct {
	    const char addressing;
	    const char opname[7];
	} bf_op[8] = {
	    { BF_ONLY, "bftst"  },
	    { BF_DREG, "bfextu" },
	    { BF_ONLY, "bfchg"  },
	    { BF_DREG, "bfexts" },
	    { BF_ONLY, "bfclr"  },
	    { BF_DREG, "bfffo"  },
	    { BF_ONLY, "bfset"  },
	    { DREG_BF, "bfins"  }
	};

	REJECT (M000|M010);
	if (BYTE3 & 0x80) {
	    UNDEFINED(); return;
	}

	code->bytes = 4;
	OPECODE (bf_op[BYTE1 & 7].opname);
	switch  (bf_op[BYTE1 & 7].addressing) {
	case BF_ONLY:				/* bf... ea{m:n} */
	    setEA (code, &code->op1, ptr, DATAREG | ((BYTE1 & 7) ? CTRLCHG : CONTROL));
	    setBitField (&code->op2, ptr);
	    break;
	case BF_DREG:				/* bf... ea{m:n},dn */
	    setEA (code, &code->op1, ptr, CONTROL | DATAREG);
	    setBitField (&code->op2, ptr);
	    setDn (&code->op3, BYTE3 >> 4);
	    break;
	case DREG_BF:				/* bf... dn,ea{m:n} */
	    setDn (&code->op1, BYTE3 >> 4);
	    setEA (code, &code->op2, ptr, CTRLCHG | DATAREG);
	    setBitField (&code->op3, ptr);
	    break;
	}
	return;
    }
#undef BF_ONLY
#undef BF_DREG
#undef DREG_BF

    {
	static const char sft_op[8][5] = {
	    "asr", "asl", "lsr", "lsl", "roxr", "roxl", "ror", "rol"
	};

	if ((WORD1 & 0xc0) == 0xc0) {		/* op.w <ea> */
	    code->size = code->size2 = WORDSIZE;
	    setEA (code, &code->op1, ptr, MEMORY & CHANGE);
	    OPECODE (sft_op[ BYTE1 & 7 ]);
	} else {
	    SETSIZE();
	    if (WORD1 & 0x20)
		setDn (&code->op1, BYTE1 >> 1);	/* op.* dm,dn */
	    else
		set18 (&code->op1, BYTE1 >> 1);	/* op.* #q,dn */
	    setDn (&code->op2, WORD1);
	    OPECODE (sft_op[((WORD1 & 0x18) >> 2) + (BYTE1 & 1)]);
	}
	return;
    }

    /* 未定義のビットパターンは存在しない. */
}


private void
setBitField (operand* op, address ptr)
{
    op->ea = BitField;
    IfNeedStr {
	char *p = op->operand;
	*p++ = '{';
	if (BYTE3 & 0x08) {
	    *p++ = 'd';
	    *p++ = ((WORD2 >> 6) & 7) + '0';
	} else
	    p = itod2 (p, (WORD2 >> 6) & 0x1f);
	*p++ = ':';
	if (WORD2 & 0x20) {
	    *p++ = 'd';
	    *p++ = (WORD2 & 7) + '0';
	} else
	    p = itod2 (p, (WORD2 & 0x1f) ? : 32);
	*p++ = '}';
	*p++ = '\0';
    }
}


static const char* ExtensionField[128] = {
    "fmove"	, "fint"   , "fsinh"	, "fintrz"  ,
    "fsqrt"	, NULL	   , "flognp1"	, NULL	    ,
    "fetoxm1"	, "ftanh"  , "fatan"	, NULL	    ,
    "fasin"	, "fatanh" , "fsin"	, "ftan"    ,
    "fetox"	, "ftwotox", "ftentox"	, NULL	    ,
    "flogn"	, "flog10" , "flog2"	, NULL	    ,
    "fabs"	, "fcosh"  , "fneg"	, NULL	    ,
    "facos"	, "fcos"   , "fgetexp"	, "fgetman" ,
    "fdiv"	, "fmod"   , "fadd"	, "fmul"    ,
    "fsgldiv"	, "frem"   , "fscale"	, "fsglmul" ,
    "fsub"	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    "fsincos"	, "fsincos", "fsincos"	, "fsincos" ,
    "fsincos"	, "fsincos", "fsincos"	, "fsincos" ,
    "fcmp"	, NULL	   , "ftst"	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,

    "fsmove"	, "fssqrt" , NULL	, NULL	    ,
    "fdmove"	, "fdsqrt" , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    "fsabs"	, NULL	   , "fsneg"	, NULL	    ,
    "fdabs"	, NULL	   , "frneg"	, NULL	    ,
    "fsdiv"	, NULL	   , "fsadd"	, "fsmul"   ,
    "fddiv"	, NULL	   , "fdadd"	, "fdmul"   ,
    "fssub"	, NULL	   , NULL	, NULL	    ,
    "fdsub"	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
    NULL	, NULL	   , NULL	, NULL	    ,
};


static const unsigned char ExtensionFlags[128] = {

    F88x|F040|F060, F88x|F4SP|F060, F88x|F4SP|F6SP, F88x|F4SP|F060,
    F88x|F040|F060, 0             , F88x|F4SP|F6SP, 0,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, 0,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, 0,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, 0,
    F88x|F040|F060, F88x|F4SP|F6SP, F88x|F040|F060, 0,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP,
    F88x|F040|F060, F88x|F4SP|F6SP, F88x|F040|F060, F88x|F040|F060,
    F88x|F4SP|F060, F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F060,
    F88x|F040|F060, 0             , 0             , 0,
    0             , 0             , 0             , 0,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP,
    F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP, F88x|F4SP|F6SP,
    F88x|F040|F060, 0             , F88x|F040|F060, 0,
    0             , 0             , 0             , 0,

         F040|F060,      F040|F060, 0             , 0,
         F040|F060,      F040|F060, 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
         F040|F060, 0             ,      F040|F060, 0,
         F040|F060, 0             ,      F040|F060, 0,
         F040|F060, 0             ,      F040|F060,      F040|F060,
         F040|F060, 0             ,      F040|F060,      F040|F060,
         F040|F060, 0             , 0             , 0,
         F040|F060, 0             , 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
    0             , 0             , 0             , 0,
};



/*
    opecode FPm,FPn が有効  +1	fxxx fpm,fpn というformatが存在しない場合
    opecode FPm     が有効  +2	fxxx fpn,fpn が fxxx fpn と解釈される場合

    ex) ftst = 2,  fmove = 1,  fabs = 1+2
*/
static const unsigned char ExtensionFormat[128] = {
	1,	3,	3,	3,
	3,	0,	3,	0,
	3,	3,	3,	0,
	3,	3,	3,	3,
	3,	3,	3,	0,
	3,	3,	3,	0,
	3,	3,	3,	0,
	3,	3,	3,	3,
	1,	1,	1,	1,
	1,	1,	1,	1,
	1,	0,	0,	0,
	0,	0,	0,	0,
	1,	1,	1,	1,
	1,	1,	1,	1,
	1,	0,	2,	0,
	0,	0,	0,	0,

	1,	3,	0,	0,
	1,	3,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0,
	3,	0,	3,	0,
	3,	0,	3,	0,
	1,	0,	1,	1,
	1,	0,	1,	1,
	1,	0,	0,	0,
	1,	0,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0,
	0,	0,	0,	0
};



/* move16用 */
INLINE private void
setAbsLong (disasm* code, operand* op, address ptr)
{
    op->opval = (address) peekl (ptr + code->bytes);

    IfNeedStr {
	char* p = op->operand;
	*p++ = '(';
	p = itox8d (p, (LONG) op->opval);
	*p++ = ')';
	*p = '\0';
    }
    op->ea = AbLong;
    op->eaadrs = PC + code->bytes;
    code->bytes += 4;
}

INLINE private void
setPMMUreg (disasm* code, operand* op, address ptr)
{

    op->ea = MMUreg;
    code->size = code->size2 = WORDSIZE;
    code->bytes = 4;

    if (MMU_type & MMU851) {
	code->mputypes = M020;

	if ((WORD2 & 0xf9ff) == 0x6000) {
	    IfNeedStr {
		strcpy (op->operand, (BYTE3 & 4) ? "pcsr" : "psr");
	    }
	    return;
	} else if ((WORD2 & 0xf9e3) == 0x7000) {
	    IfNeedStr {
		strcpy (op->operand, (BYTE3 & 4) ? "bac0" : "bad0");
		op->operand[3] += (WORD2 >> 2) & 7;
	    }
	    return;
	}
    }

    else if (MMU_type & MMU030) {
	/* MC68EC030 では PMOVE to/from ACUSR(実体は同じものらしい) */
	if ((WORD2 & 0xfdff) == 0x6000) {
	    IfNeedStr {
		strcpy (op->operand, "mmusr");
	    }
	    code->mputypes = M030;
	    return;
	}
    }

    UNDEFINED(); return;
}


private void
op0f (address ptr, disasm* code)
{

    if (BYTE1 == 0xf8) {

	if (WORD1 == 0xf800 && WORD2 == 0x01c0) {
	    if (MPU_types & M060)
		REJECT (~M060);		/* -m68060,cpu32 なら 68060 を優先 */
	    else if (!Disasm_CPU32) {
		UNDEFINED(); return;
	    }

	    OPECODE ("lpstop");
	    code->bytes = 4;
	    setIMD (code, &code->op1, ptr + 2, WORDSIZE);
	    code->size = WORDSIZE;
	    return;
	}

	if (Disasm_CPU32
	 && (WORD1 & 0xffc0) == 0xf800
	 && (WORD2 & 0x8238) == 0
	 && ((WORD2 >> 6) & 3) != 3) {

	    IfNeedStr {
		strcpy (code->opecode, "tblun");
		if (WORD2 & 0x0800)
		    code->opecode[3] = 's';	/* Signed */
		if ((WORD2 & 0x0400) == 0)
		    code->opecode[4] = '\0';	/* Result rounded */
	    }
	    code->bytes = 4;
	    code->size = code->size2 = (WORD2 >> 6) & 3;

	    if (BYTE3 & 1) {			/* <ea>,dx */
		/* リファレンスマニュアルには制御可変と書いてあ	*/
		/* るが表の内容は制御可変ではない.		*/
		/* おまけに Signed では (an) が使えないとなって	*/
		/* いる. これは間違いではないのか？		*/
		int eamode = CONTROL | PREDEC;
		if (WORD2 & 0x0800)
		    eamode &= ~ADRIND;
		setEA (code, &code->op1, ptr, eamode);
	    } else				/* dym:dyn,dx */
		setPairDn (&code->op1, WORD1 & 7, WORD2 & 7);
	    setDn (&code->op2, BYTE3 >> 4);
	    return;
	}
    }

    /* 68040, 68060 only */
    if (MPU_types & (M040|M060)) {
	code->mputypes &= (M040|M060);

	switch (BYTE1) {
	case 0xf4:
	    {
		static const char cachekind[4][4] = {
		    "nc", "dc", "ic", "bc"
		};

		IfNeedStr {
		    strcpy (code->opecode, (WORD1 & 0x20) ? "cpush?" : "cinv?");
		    code->opecode[(WORD1 & 0x20) ? 5 : 4] = "?lpa"[(WORD1 >> 3) & 3];
		    strcpy (code->op1.operand, cachekind[(WORD1 >> 6) & 3]);
		}
		code->op1.ea = CtrlReg;
		switch ((WORD1>>3) & 3) {
		case 0:
		    UNDEFINED(); return;
		case 1:
		case 2:
		    IfNeedStr {
			strcpy (code->op2.operand, "(a0)");
			code->op2.operand[2] += WORD1 & 7;
		    }
		    code->op2.ea = AregID;
		case 3:
		    break;
		}
	    }
	    return;

	case 0xf5:
	    if ((MMU_type & (MMU040|MMU060)) == 0) {
		UNDEFINED(); return;
	    }

	    if ((WORD1 & 0xe0) == 0) {
		OPECODE ("pflusha");
		if (WORD1 & (1 << 4)) {			/* pflusha(n) */
		    if (WORD1 & 7) {
			UNDEFINED(); return;		/* 未定義 reg.field */
		    }
		} else {				/* pflush(n) (an) */
		    IfNeedStr {
			code->opecode[6] = '\0';
			strcpy (code->op1.operand, "(a0)");
			code->op1.operand[2] += WORD1 & 7;
		    }
		    code->op1.ea = AregID;
		}
		if ((WORD1 & (1 << 3)) == 0)
		    strcat (code->opecode, "n");
		return;
	    }

	    if ((WORD1 & 0xd8) == 0x48) {
		IfNeedStr {
		    strcpy (code->opecode, "ptestw");
		    if (WORD1 & 0x20)
			code->opecode[5] = 'r';
		    strcpy (code->op1.operand, "(a0)");
		    code->op1.operand[2] += WORD1 & 7;
		}
		code->op1.ea = AregID;
		return;
	    }
	    if ((WORD1 & 0xb0) == 0x80) {
		IfNeedStr {
		    strcpy (code->opecode, "plpaw");
		    if (WORD1 & 0x40)
			code->opecode[4] = 'r';
		    strcpy (code->op1.operand, "(a0)");
		    code->op1.operand[2] += WORD1 & 7;
		}
		code->op1.ea = AregID;
		return;
	    }
	    break;

	case 0xf6:
	    OPECODE ("move16");
	    switch ((WORD1 >> 3) & 0x1f) {
	    case 0:
	    case 2:
		IfNeedStr {
		    strcpy (code->op1.operand, "(a0)+");
		    code->op1.operand[2] += WORD1 & 7;
		    if (WORD1 & (2 << 3))
			code->op1.operand[4] = '\0';
		}
		setAbsLong (code, &code->op2, ptr);
		return;

	    case 1:
	    case 3:
		IfNeedStr {
		    strcpy (code->op2.operand, "(a0)+");
		    code->op2.operand[2] += WORD1 & 7;
		    if (WORD1 & (2 << 3))
			code->op2.operand[4] = '\0';
		}
		setAbsLong (code, &code->op1, ptr);
		return;

	    case 4:
		if ((WORD2 & 0x8fff) != 0x8000) {
		    UNDEFINED(); return;
		}
		IfNeedStr {
		    strcpy (code->op1.operand, "(a0)+");
		    code->op1.operand[2] += WORD1	 & 7;
		    strcpy (code->op2.operand, "(a0)+");
		    code->op2.operand[2] += (BYTE3 >> 4) & 7;
		}
		code->op1.ea = code->op2.ea = AregIDPI;
		code->bytes = 4;
		return;

	    default:
		break;
	    }

	/* 0xf8 は上で処理済み */

	default:
	    break;
	}
    }
    code->mputypes = ~0;


    /* 68020+68851,68030(on chip MMU) */
    if ((MMU_type & (MMU851|MMU030)) && ((WORD1 & 0xffc0) == 0xf000)) {
	code->mputypes &= (M020|M030);

	switch ((BYTE3 >> 5) & 7) {
	case 1:
	    {
		int pmode = (BYTE3 >> 2) & 7;

		switch (pmode) {
		case 0:
		    if (WORD2 & 0x01e0) {
			UNDEFINED(); return;
		    }
		    IfNeedStr {
			strcpy (code->opecode, "ploadw");
			if (BYTE3 & 2)
			    code->opecode[5] = 'r';
		    }
		    code->bytes = 4;
		    setMMUfc (code, &code->op1, WORD2);
		    setEA (code, &code->op2, ptr, CTRLCHG);
		    return;
		case 1:
		    if (WORD2 != 0x2400)
			break;
		    OPECODE ("pflusha");
		    code->bytes = 4;
		    return;
		case 2:
		case 3:
		    if ((MMU_type & MMU851) == 0)
			break;
		    code->mputypes = M020;		/* 68020 + 68851 only */

		    if (pmode == 2) {
			if (WORD2 != 0x2800)
			    break;
			code->bytes = 4;
			IfNeedStr {
			    strcpy (code->opecode, "pvalid");
			    strcpy (code->op1.operand, "val");
			}
			code->op1.ea = MMUreg;
		    } else {
			if ((WORD2 & 0xfff8) != 0x2c00)
			    break;
			code->bytes = 4;
			OPECODE ("pvalid");
			setAn (&code->op1, WORD2);
		    }
		    setEA (code, &code->op2, ptr, CTRLCHG);
		    code->size = code->size2 = code->default_size = LONGSIZE;
		    return;
		case 4:
		case 6:
		    if (BYTE3 & 2)
			break;
		    code->bytes = 4;
		    OPECODE ("pflush");
		    setMMUfc (code, &code->op1, WORD2);			/* fc */
		    setMMUfc (code, &code->op2, (WORD2 >> 5) | 0x10);	/* #xx */
		    if (pmode == 6)
			setEA (code, &code->op3, ptr, CTRLCHG);		/* <ea> */
		    return;
		case 5:
		case 7:
		    if (BYTE3 & 2)
			break;
		    REJECTnoPMMU();
		    code->bytes = 4;
		    OPECODE ("pflushs");
		    setMMUfc (code, &code->op1, WORD2);			/* fc */
		    setMMUfc (code, &code->op2, (WORD2 >> 5) | 0x10);	/* #xx */
		    if (pmode == 7)
			setEA (code, &code->op3, ptr, CTRLCHG);		/* <ea> */
		    return;
		}
	    }
	    break;

	case 0:	/* MC68EC030 では P-REGISTER = 000/001 が PMOVE to/from ACx */
	case 2:
	    if (BYTE4) {
		UNDEFINED(); return;
	    }

	    switch (BYTE3 & 3) {
	    case 0:			/* MEMORY to MMUreg with FLUSH */
	    case 2:			/* MMUreg to MEMORY with FLUSH */
		OPECODE ("pmove");
		break;
	    case 1:			/* MEMORY to MMUreg flush disable */
		if ((MMU_type & MMU030) == 0)
		    break;
		code->mputypes = M030;		/* pmovefd is 68030 only */

		OPECODE ("pmovefd");
		break;
	    case 3:			/* MMUreg to MEMORY flush disable */
		UNDEFINED(); return;
	    }

	    {
		static const struct {
		    mputypes	mpu;
		    char	size;
		    char	name[6];
		} mmu_regs[16] = {
		    { 0,		0,	  ""	},
		    { 0,		0,	  ""	},
		    { M030,		LONGSIZE, "tt0"	},
		    { M030,		LONGSIZE, "tt1"	},
		    { 0,		0,	  ""	},
		    { 0,		0,	  ""	},
		    { 0,		0,	  ""	},
		    { 0,		0,	  ""	},
		    { M020|M030|M040,	LONGSIZE, "tc"	},
		    { M020,		QUADSIZE, "drp"	},
		    { M020|M030|M040,	QUADSIZE, "srp"	},
		    { M020|M030,	QUADSIZE, "crp"	},
		    { M020,		BYTESIZE, "cal"	},
		    { M020,		BYTESIZE, "val"	},
		    { M020,		BYTESIZE, "scc"	},
		    { M020,		WORDSIZE, "ac"	}
		};
		int n = ((BYTE3 >> 2) & 7) + ((BYTE3 >> 3) & 8);
		const char* p = mmu_regs[n].name;

		REJECT (~mmu_regs[n].mpu);
		code->size = code->size2 = code->default_size = mmu_regs[n].size;

		if (BYTE3 & 2) {		/* MMUreg to MEMORY */
		    int ea = (WORD1 >> 3) & 7;

		    if (ea == 7)
			ea = (WORD1 & 7) + 8;
		    if ((1 << ea) & (CHANGE ^ CTRLCHG))
			REJECTnoPMMU ();

		    IfNeedStr {
			strcpy (code->op1.operand, p);
		    }
		    code->op1.ea = MMUreg;
		    code->bytes = 4;
		    n = ((MMU_type & MMU851) ? CHANGE : 0)	/* 68851=可変       */
		      | ((MMU_type & MMU030) ? CTRLCHG : 0);	/* 68030=制御・可変 */
		    if (code->size == QUADSIZE)
			n &= ~(DATAREG | ADRREG);	/* crp,drp,srp -> dn,an は不可 */
		    setEA (code, &code->op2, ptr, n);
		    return;
		} else {			/* MEMORY to MMUreg */
		    int ea = (WORD1 >> 3) & 7;

		    if (ea == 7)
			ea = (WORD1 & 7) + 8;
		    if ((1 << ea) & (ALL ^ CTRLCHG))
			REJECTnoPMMU ();

		    IfNeedStr {
			strcpy (code->op2.operand, p);
		    }
		    code->op2.ea = MMUreg;
		    code->bytes = 4;
		    n = ((MMU_type & MMU851) ? ALL : 0)		/* 68851=全て       */
		      | ((MMU_type & MMU030) ? CTRLCHG : 0);	/* 68030=制御・可変 */
		    if (code->size == QUADSIZE)
			n &= ~(DATAREG | ADRREG);	/* dn,an -> crp,drp,srp は不可 */
		    setEA (code, &code->op1, ptr, n);
		    return;
		}
	    }
	case 3:
	    OPECODE ("pmove");
	    setPMMUreg (code, ((BYTE3 & 2) ? &code->op1 : &code->op2), ptr);
	    setEA      (code, ((BYTE3 & 2) ? &code->op2 : &code->op1), ptr, CTRLCHG);
	    return;
	case 4:
	    IfNeedStr {
		strcpy (code->opecode, "ptestw");
		if (BYTE3 & 2)
		    code->opecode[5] = 'r';
	    }
	    code->bytes = 4;
	    setMMUfc (code, &code->op1, WORD2);
	    setEA (code, &code->op2, ptr, CTRLCHG);
	    set07 (&code->op3, (BYTE3 >> 2));
	    if (BYTE3 & 1)
		setAn (&code->op4, WORD2 >> 5);
	    return;
	case 5:
	    if (WORD2 != 0xa000) {
		UNDEFINED(); return;
	    }
	    if ((MMU_type & MMU851) == 0)
		break;
	    code->mputypes = M020;

	    OPECODE ("pflushr");
	    code->bytes = 4;
	    setEA (code, &code->op1, ptr, MEMORY);
	    return;
	case 6:
	case 7:
	    break;
	}
    }

    /* 68020+68851 */
    if ((MMU_type & MMU851) && (BYTE1 & 0x0e) == 0) {
	int temp = (WORD1 >> 6) & 7;

	code->mputypes = M020;

	switch (temp) {
	case 0:		/* 解析済 */
	    break;
	case 1:		/* PCss, PDBcc, PTRAPcc */
	    if (WORD2 & 0xffc0) {
		UNDEFINED(); return;
	    }
	    switch ((WORD1>>3) & 7) {
	    case 1:
		IfNeedStr {			/* PDBcc */
		    strcpy (code->opecode, "pdb");
		    MMUCOND (code, WORD2);
		}
		setDn (&code->op1, WORD1);
		setrelative4 (code->op2.operand, SignWORD3, &code->op2.opval);
		code->jmp = code->op2.opval;
		code->jmpea = code->op2.ea = PCDISP;
		code->op2.labelchange1 = -1;	/* TRUE */
		code->flag = BCCOP;
		code->bytes = 6;
		code->size = code->size2 = WORDSIZE;	/* default_size も word */
		break;
	    case 7:
		code->bytes = 4;			/* PTRAPcc */

		switch (WORD1 & 7) {
		case 2:
		    code->size = WORDSIZE;
		    setIMD (code, &code->op1, ptr+2, WORDSIZE);
		    break;
		case 3:
		    code->size = LONGSIZE;
		    setIMD (code, &code->op1, ptr+2, LONGSIZE);
		    break;
		case 4:
		    break;
		default:
		    UNDEFINED(); return;
		}
		IfNeedStr {
		    strcpy (code->opecode, "ptrap");
		    MMUCOND (code, WORD2);
		}
#if 0
		code->default_size = NOTHING;
#endif
		break;
	    default:
		IfNeedStr {			/* PScc */
		    strcpy (code->opecode, "ps");
		    MMUCOND (code, WORD2);
		}
		code->bytes = 4;
		code->size = code->size2 = code->default_size = BYTESIZE;
		setEA (code, &code->op1, ptr, DATA & CHANGE);
		break;
	    }
	    return;
	case 2:
	case 3:
	    IfNeedStr {				/* PBcc */
		strcpy (code->opecode, "pb");
		MMUCOND (code, WORD1);
	    }
	    switch (temp) {
	    case 2:
		code->bytes = 4;
		code->size = code->size2 = WORDSIZE;
		setrelative (code->op1.operand, SignWORD2, &code->op1.opval);
		break;
	    case 3:
		code->bytes = 6;
		code->size = code->size2 = LONGSIZE;
		setlongrelative (code->op1.operand, SignLONG05, &code->op1.opval);
		break;
	    }
#if 0
	    code->default_size = NOTHING;
#endif
	    code->jmp = code->op1.opval;
	    code->jmpea = code->op1.ea = PCDISP;
	    code->flag = BCCOP;
	    code->op1.labelchange1 = -1;	/* TRUE */
	    return;
	case 4:
	    OPECODE ("psave");
	    setEA (code, &code->op1, ptr, CTRLCHG | PREDEC);
	    return;
	case 5:
	    OPECODE ("prestore");
	    setEA (code, &code->op1, ptr, CONTROL | POSTINC);
	    return;
	case 6:
	case 7:
	default:
	    UNDEFINED(); return;
	}
    }



/*

	浮動小数点命令

	FPUID_table[ID*2] が真なら、有効な命令である.

	-m68000/010 指定時には FPCP_type = 0 になるので、FPCP_type が
	真ならば必ず -m68020 以上である. よって、MPU_types の検査は不要.

*/

    if (FPCP_type && FPUID_table[BYTE1 & 0x0e]) {

	code->mputypes = ~(M000|M010);
	code->fpuid = (BYTE1 & 0x0e) >> 1;
	code->default_size = EXTENDSIZE;

	switch ((WORD1 >> 6) & 7) {
	case 0:				/* type 000(一般命令) */
	    switch (BYTE3 >> 5) {	/* opclass */
	    case 0:			/* FPm to FPn */
		if (BYTE2) {
		    UNDEFINED(); return;
		}

		FPOPESET;
		code->bytes = 4;
		code->size = code->size2 = EXTENDSIZE;
		setFPn (&code->op1, BYTE3 >> 2);

		if ((WORD2 & 0x78) == 0x30) {		/* fsincos */
		    IfNeedStr {
			strcpy (code->op2.operand, "fp0:fp0");
			code->op2.operand[2] += WORD2	     & 7;
			code->op2.operand[6] += (WORD2 >> 7) & 7;
		    }
		    return;
		} else {
		    if (ExtensionFormat[WORD2 & 0x7f] & 1) {
			if ((ExtensionFormat[ WORD2 & 0x7f ] & 2)
			 && (((BYTE3 >> 2) & 7) == ((WORD2 >> 7) & 7)))
			    ;	/* f??? fpn */
				/* fxxx fpn,fpn が fxxx fpn となる場合 */
			else
			    setFPn (&code->op2, WORD2 >> 7);

		    } else if ((UndefRegLevel & 1)
			&& ((WORD2 >> 7) & 7) != 0
			&& ((WORD2 >> 7) & 7) != ((BYTE3 >> 2) & 7)
		    ) {
			UNDEFINED(); return;
		    }
		    return;
		}
	    case 1:				/* undefined, reserved */
		UNDEFINED(); return;
	    case 2:				/* Memory to FPn or movecr */
		if (((BYTE3 >> 2) & 7) == 7) {		/* movecr */
		    REJECTnoFPSP ();
		    code->bytes = 4;
		    code->size = code->size2 = EXTENDSIZE;
		    IfNeedStr {
			strcpy (code->opecode, "fmovecr");
			code->op1.operand[0] = '#';
			itox2d (code->op1.operand + 1, WORD2 & 0x7f);
		    }
		    code->op1.ea = IMMED;
		    code->op1.opval = (address) (WORD2 & 0x7f);
		    setFPn (&code->op2, WORD2 >> 7);
		    return;
		} else {				/* Memory to FPn */
		    static const int fpsize2size[7] = {
			LONGSIZE  , SINGLESIZE, EXTENDSIZE,
			PACKEDSIZE, WORDSIZE  , DOUBLESIZE,
			BYTESIZE
		    };
		    static const int fpsize2sea[7] = {
			DATA,   DATA, MEMORY,
			MEMORY, DATA, MEMORY,
			DATA
		    };

		    FPOPESET;
		    code->bytes = 4;
		    code->size = code->size2 = fpsize2size[(BYTE3 >> 2) & 7];
		    if (code->size == PACKEDSIZE)
			REJECTnoFPSP ();
		    setEA (code, &code->op1, ptr, fpsize2sea[(BYTE3 >> 2) & 7]);

		    if ((WORD2 & 0x78) == 0x30) {	/* fsincos */
			IfNeedStr {
			    strcpy (code->op2.operand, "fp0:fp0");
			    code->op2.operand[2] += (WORD2 & 7);
			    code->op2.operand[6] += ((WORD2 >> 7) & 7);
			}
		    } else {
			if (ExtensionFormat[ WORD2 & 0x7f ] & 1)
			    setFPn (&code->op2, WORD2 >> 7);
			else if ((WORD2 >> 7) & 7) {	/* should be zero ... */
			    UNDEFINED(); return;
			}
		    }
		    return;
		}
	    case 3:					/* move FPn to ... */
		OPECODE ("fmove");
		code->bytes = 4;
		setFPn (&code->op1, WORD2 >> 7);

		switch ((BYTE3 >> 2) & 7) {		/* destination format */
		case 0:		/* Long */
		    code->size = code->size2 = LONGSIZE;
		    setEA (code, &code->op2, ptr, DATA & CHANGE);
		    break;
		case 1:		/* Single */
		    code->size = code->size2 = SINGLESIZE;
		    setEA (code, &code->op2, ptr, DATA & CHANGE);
		    break;
		case 2:		/* Extend */
		    code->size = code->size2 = EXTENDSIZE;
		    setEA (code, &code->op2, ptr, (DATA & CHANGE) ^ DATAREG);
		    break;
		case 3:		/* Packed with Static K-Factor */
		    REJECTnoFPSP ();
		    code->size = code->size2 = PACKEDSIZE;
		    setEA (code, &code->op2, ptr, (DATA & CHANGE) ^ DATAREG);
		    code->op3.ea = KFactor;
		    IfNeedStr {
			UWORD factor = WORD2 & 0x7f;
			char *p = code->op3.operand;

			*p++ = '{';
			*p++ = '#';
			if (factor > 0x3f) {
			    *p++ = '-';
			    factor = -factor + 0x80;
			}
			p = itod2 (p, factor);
			*p++ = '}';
			*p = '\0';
		    }
		    break;
		case 4:		/* Word */
		    code->size = code->size2 = WORDSIZE;
		    setEA (code, &code->op2, ptr, DATA & CHANGE);
		    break;
		case 5:		/* Double */
		    code->size = code->size2 = DOUBLESIZE;
		    setEA (code, &code->op2, ptr, (DATA & CHANGE) ^ DATAREG);
		    break;
		case 6:		/* Byte */
		    code->size = code->size2 = BYTESIZE;
		    setEA (code, &code->op2, ptr, DATA & CHANGE);
		    break;
		case 7:		/* Packed with Dynamic K-Factor */
		    REJECTnoFPSP ();
		    code->size = code->size2 = PACKEDSIZE;
		    setEA (code, &code->op2, ptr, (DATA & CHANGE) ^ DATAREG);
		    code->op3.ea = KFactor;
		    IfNeedStr {
			strcpy (code->op3.operand, "{d0}");
			code->op3.operand[2] += (WORD2 >> 4) & 7;
		    }
		    break;
		}
		return;
	    case 4:		/* move(m) Mem to FPCR/FPSR,FPIAR */
		{
		    int regno = (BYTE3 >> 2) & 7;
		    adrmode addressing = setFPCRSRlist (&code->op2, regno);

		    if (addressing == 0 || (WORD2 & 0x03ff) != 0) {
			UNDEFINED(); return;
		    }
		    IfNeedStr {
			strcpy (code->opecode, "fmovem");
			if (regno == 1 || regno == 2 || regno == 4)
			    code->opecode[5] = '\0';	/* fmovem -> fmove */
		    }
		    code->bytes = 4;
		    code->size = code->size2 = code->default_size = LONGSIZE;
		    setEA (code, &code->op1, ptr, addressing);
		    if (code->op1.ea == IMMED) {
			switch (regno) {
			case 3:				/* fmovem.l #imm,#imm,reg */
			case 5:
			case 6:
			    REJECTnoFPSP ();
			    setEA (code, &code->op2, ptr, addressing);
			    setFPCRSRlist (&code->op3, regno);
			    break;
			case 7:				/* fmovem.l #imm,#imm,#imm,reg */
			    REJECTnoFPSP();
			    setEA (code, &code->op2, ptr, addressing);
			    setEA (code, &code->op3, ptr, addressing);
			    setFPCRSRlist (&code->op4, regno);
			    break;
			default:
			    break;
			}
		    }
		    return;
		}
	    case 5:		/* move(m) FPCR/FPSR,FPIAR to Mem */
		{
		    adrmode addressing = setFPCRSRlist (&code->op1, BYTE3 >> 2);

		    if (addressing == 0 || (WORD2 & 0x03ff) != 0) {
			UNDEFINED(); return;
		    }
		    IfNeedStr {
			int mode;

			strcpy (code->opecode, "fmovem");
			if ((mode = (BYTE3 >> 2) & 7) == 1 || mode == 2 || mode == 4)
			    code->opecode[5] = '\0';	/* fmovem -> fmove */
		    }
		    code->bytes = 4;
		    code->size = code->size2 = code->default_size = LONGSIZE;
		    setEA (code, &code->op2, ptr, addressing & CHANGE);
		    return;
		}
	    case 6:		/* fmovem.x Mem to FPCP */
		if ((BYTE3 & 7) == 0) {
		    OPECODE ("fmovem");			/* from MEMORY to FPCP */
		    code->bytes = 4;
		    code->size = code->size2 = EXTENDSIZE;

		    switch ((BYTE3 >> 3) & 3) {
		    case 0:
		    case 1:
			UNDEFINED(); return;
		    case 2:
			setFPreglist (code->op2.operand, ptr);
			break;
		    case 3:
			if (WORD2 & 0x8f) {
			    UNDEFINED(); return;
			}
			REJECTnoFPSP ();
			setDn (&code->op2, WORD2 >> 4);
		    }
		    setEA (code, &code->op1, ptr, CONTROL | POSTINC);
		    return;
		}
		break;
	    case 7:		/* fmovem.x FPCP to Mem */
		if ((BYTE3 & 7) == 0) {
		    OPECODE ("fmovem");			/* from FPCP to MEMORY */
		    code->bytes = 4;
		    code->size = code->size2 = EXTENDSIZE;
		    switch ((BYTE3 >> 3) & 3) {
		    case 0:
		    case 2:
			setFPreglist (code->op1.operand, ptr);
			break;
		    case 1:
		    case 3:
			if (WORD2 & 0x8f) {
			    UNDEFINED(); return;
			}
			REJECTnoFPSP ();
			setDn (&code->op1, WORD2 >> 4);
		    }
		    setEA (code, &code->op2, ptr, CTRLCHG | PREDEC);
		    return;
		}
		break;
	    }

	case 1:				/* type 001(FDBcc/FScc/FTRAPcc) */
	    if (WORD2 & 0xffc0) {
		UNDEFINED(); return;
	    }
	    REJECT060noFPSP ();		/* 68060 のみ Software Emulation */

	    if ((WORD1 & 0x38) == 0x08) {	/* FDBcc */
		IfNeedStr {
		    if (Disasm_Dbra && (WORD2 & 0x1f) == 0)
			strcpy (code->opecode, "fdbra");
		    else {
			strcpy (code->opecode, "fdb");
			FPCOND (code, WORD2);
		    }
		}
		setDn (&code->op1, WORD1);
		setrelative4 (code->op2.operand, SignWORD3, &code->op2.opval);
		code->default_size = NOTHING;
		code->jmp = code->op2.opval;
		code->jmpea = code->op2.ea = PCDISP;
		code->op2.labelchange1 = -1;	/* TRUE */
		code->flag = BCCOP;
		code->bytes = 6;
		return;
	    }

	    if (0x7a < BYTE2) {			/* FScc */
		IfNeedStr {
		    strcpy (code->opecode, "fs");
		    FPCOND (code, WORD2);
		}
		code->bytes = 4;
		code->size = code->size2 = code->default_size = BYTESIZE;
		setEA (code, &code->op1, ptr, DATA & CHANGE);
		return;
	    }

	    if (BYTE2 < 0x7d) {			/* FTRAPcc */
		code->bytes = 4;
		if ((WORD1 & 7) != 4) {
		    code->size = ((WORD1 & 7) == 2) ? WORDSIZE : LONGSIZE;
		    setIMD (code, &code->op1, ptr + 2, code->size);
		}
		IfNeedStr {
		    strcpy (code->opecode, "ftrap");
		    FPCOND (code, WORD2);
		}
#if 0
		code->default_size = NOTHING;
#endif
		return;
	    }
	    break;

	case 2:				/* type 010(FBcc.W) */
	    if ((BYTE2 == 0x80) && (WORD2 == 0x00)) {
		OPECODE ("fnop");
		code->bytes = 4;
		return;
	    }
	    /* fall through */
	case 3:				/* type 011(FBcc.L) */
	    if ((WORD1 & 0x1f) == 15) {
		IfNeedStr {
		    strcpy (code->opecode, "fbra");
		}
		code->flag = JMPOP;
	    } else {
		IfNeedStr {
		    strcpy (code->opecode, "fb");
		    FPCOND (code, WORD1);
		}
		code->flag = BCCOP;
	    }
	    if (WORD1 & 0x40) {
		code->bytes = 6;
		code->size = code->size2 = LONGSIZE;
		setlongrelative (code->op1.operand, SignLONG05, &code->op1.opval);
	    } else {
		code->bytes = 4;
		code->size = code->size2 = WORDSIZE;
		setrelative (code->op1.operand, SignWORD2, &code->op1.opval);
	    }
#if 0
	    code->default_size = NOTHING;
#endif
	    code->jmp = code->op1.opval;
	    code->jmpea = code->op1.ea = PCDISP;
	    code->op1.labelchange1 = -1;	/* TRUE */
	    return;

	case 4:				/* type 100(FSAVE) */
	    OPECODE ("fsave");
	    setEA (code, &code->op1, ptr, CTRLCHG | PREDEC);
	    return;

	case 5:				/* type 101(FRESTORE) */
	    OPECODE ("frestore");
	    setEA (code, &code->op1, ptr, CONTROL | POSTINC);
	    return;

	case 6:				/* type 110(未定義命令)	*/
	case 7:				/* type 111(〃)		*/
	    break;
	}
	UNDEFINED(); return;
    }
    code->mputypes = ~0;


#ifndef	OSKDIS
    /* F line Emulator($ffxx:DOSコール、$fexx:FPACKコール) */

    switch (BYTE1) {
    case 0xff:
	if (OSlabel && OSlabel[BYTE2]) {
	    if (BYTE2 == DOS_EXIT || BYTE2 == DOS_EXIT2 || BYTE2 == DOS_KEEPPR
#ifdef	DOS_KILL_PR_IS_RTSOP
	     || BYTE2 == DOS_KILL_PR
#endif
	     ) {
		code->flag = RTSOP;
		code->opflags += FLAG_NEED_NULSTR;
	    }
	    IfNeedStr {
		strcpy (code->opecode, OSCallName);
		strcpy (code->op1.operand, OSlabel[BYTE2]);
		code->opflags += FLAG_CANNOT_UPPER;
	    }
	    code->size = code->size2 = code->default_size = NOTHING;
	    return;
	}
	break;
    case 0xfe:
	if (FElabel && FElabel[BYTE2]) {
	    IfNeedStr {
		strcpy (code->opecode, FECallName);
		strcpy (code->op1.operand, FElabel[BYTE2]);
		code->opflags += FLAG_CANNOT_UPPER;
	    }
	    code->size = code->size2 = code->default_size = NOTHING;
	    return;
	}
	break;
    default:
	break;
    }
#endif	/* !OSKDIS */

    if (Disasm_UnusedTrapUndefined) {
	UNDEFINED(); return;	/* 未使用のF line及びA lineを未定義命令とする */
    } else {
	IfNeedStr {		/* そうでなければ無理矢理命令にする */
	    strcpy (code->opecode, DC_WORD);
	    itox4d (code->op1.operand, WORD1);
	}
	code->size = code->size2 = code->default_size = WORDSIZE;
    }

}


private void
setMMUfc (disasm* code, operand* op, int fc)
{
    char* p = op->operand;

    switch ((fc >> 3) & 3) {
    case 0:
	op->ea = MMUreg;
	switch (fc & 7) {
	case 0:
	    IfNeedStr {
		strcpy (p, "sfc");
	    }
	    break;
	case 1:
	    IfNeedStr {
		strcpy (p, "dfc");
	    }
	    break;
	default:
	    UNDEFINED(); return;
	}
	break;
    case 1:
	op->ea = DregD;
	IfNeedStr {
	    *p++ = 'd';
	    *p++ = (fc & 7) + '0';
	    *p++ = '\0';
	}
	break;
    case 3:
	REJECTnoPMMU ();	/* #xx が 8～15 なら 68020 */
	/* fall through */
    case 2:
	op->ea = IMMED;
	IfNeedStr {
	    *p++ = '#';
	    fc &= 15;
	    if (fc >= 10) {
		*p++ = '1';
		fc -= 10;
	    }
	    *p++ = fc + '0';
	    *p++ = '\0';
	}
	break;
    }
}


/*  add sub 用  */
private void
addsubope (address ptr, disasm* code, const char* opname)
{

    OPECODE (opname);

    /* adda、suba */
    if ((WORD1 & 0xc0) == 0xc0) {
	code->size = code->size2 = (BYTE1 & 1) ? LONGSIZE : WORDSIZE;
	setEA (code, &code->op1, ptr, ALL);
	setAn (&code->op2, BYTE1 >> 1);
	IfNeedStr {
	    /* バイトサイズは存在しないのでコメントは不要 */
	    if (Disasm_MnemonicAbbreviation) {
		/* adda/suba #imm で 1 <= imm <= 8 なら */
		/* imm にサイズを付ける(最適化対策)	*/
		if (code->op1.ea == IMMED && ((ULONG)code->op1.opval - 1) <= 7)
		    addsize (code->op1.operand, code->size);
	    }
	    else
		strcat (code->opecode, "a");
	}
	return;
    }

    /* addx、subx */
    if ((WORD1 & 0x130) == 0x100) {
	SETSIZE ();
	IfNeedStr {
	    strcat (code->opecode, "x");
	}
	if (WORD1 & 8) {
	    IfNeedStr {
		strcpy (code->op1.operand, "-(a0)");
		strcpy (code->op2.operand, "-(a0)");
		code->op1.operand[3] += WORD1	     & 7;
		code->op2.operand[3] += (BYTE1 >> 1) & 7;
	    }
	    code->op1.ea = code->op2.ea = AregIDPD;
	}
	else {
	    setDn (&code->op1, WORD1);
	    setDn (&code->op2, BYTE1 >> 1);
	}
	return;
    }

    /* add、sub */
    SETSIZE ();
    if (BYTE1 & 1) {
	setDn (&code->op1, BYTE1 >> 1);
	setEA (code, &code->op2, ptr, MEMORY & CHANGE);
    } else {
	REJECTBYTESIZE ();
	setEA (code, &code->op1, ptr, ALL);
	setDn (&code->op2, BYTE1 >> 1);

	if (Disasm_String && code->op1.ea == IMMED) {
	    /* add.b、sub.b #imm にはコメントを付ける */
	    if (code->size == BYTESIZE)
		code->opflags += FLAG_NEED_COMMENT;

	    /* add/sub #imm,dn (1 <= imm <= 8) なら、サイズ */
	    /* を明示して addq/subq に最適化されるのを防ぐ  */
	    if ((code->size == BYTESIZE && (BYTE4 - 1) <= 7)
	     || (code->size == WORDSIZE && (WORD2 - 1) <= 7)
	     || (code->size == LONGSIZE && (LONG05 - 1) <= 7))
		addsize (code->op1.operand, code->size);
	}
    }
}


/*
 * 最適化防止用に即値オペランドにサイズを付ける
 */
private void
addsize (char* optr, opesize size)
{
    strcat (optr, (size == BYTESIZE) ? ".b" :
		  (size == WORDSIZE) ? ".w" : ".l");
}


/*  bcd 用  */
private void
bcdope (address ptr, disasm* code, const char* opname)
{
    OPECODE (opname);

    code->size = code->size2 = code->default_size = BYTESIZE;
    if (WORD1 & 8) {
	IfNeedStr {
	    strcpy (code->op1.operand, "-(a0)");
	    strcpy (code->op2.operand, "-(a0)");
	    code->op1.operand[3] += WORD1	 & 7;
	    code->op2.operand[3] += (BYTE1 >> 1) & 7;
	}
	code->op1.ea = code->op2.ea = AregIDPD;
    } else {
	setDn (&code->op1, WORD1);
	setDn (&code->op2, BYTE1 >> 1);
    }
}


/*  レジスタリスト (movem) */
private void
setreglist (char* operandstr, address ptr)
{
    unsigned int field, field2;
    boolean flag, already;
    int i, start = 0;

    if (!Disasm_String)
	return;

    if ((WORD1 & 0x38) == 0x20) {	/* pre-decrement ? */
	field = 0;
	for (i = 0; i < 16; i++)
	    field |= (WORD2 & pow2[i]) ? pow2[15 - i] : 0;
    } else
	field = WORD2;

    field2 = field & 0xff;		/* lower 8 bit */
    for (i = 0, flag = FALSE, already = FALSE; i < 9; i++) {
	if (!flag && field2 & pow2[i]) {
	    start = i;
	    flag = TRUE;
	} else {
	    if (flag && (field2 & pow2[i]) == 0) {
		char* p = strend (operandstr);
		if (already)
		    *p++ = '/';
		*p++ = 'd';
		*p++ = start + '0';
		if (start != (i - 1)) {
		    *p++ = '-';
		    *p++ = 'd';
		    *p++ = (i - 1) + '0';
		}
		*p = '\0';
		already = TRUE;
		flag = FALSE;
	    }
	}
    }

    field2 = field >> 8;
    for (i = 0, flag = FALSE; i < 9; i++) {
	if (!flag && field2 & pow2[i]) {
	    start = i;
	    flag = TRUE;
	} else {
	    if (flag && (field2 & pow2[i]) == 0) {
		char* p = strend (operandstr);
		if (already)
		    *p++ = '/';
		*p++ = 'a';
		*p++ = start + '0';
		if (start != (i - 1)) {
		    *p++ = '-';
		    *p++ = 'a';
		    *p++ = (i - 1) + '0';
		}
		*p = '\0';
		already = TRUE;
		flag = FALSE;
	    }
	}
    }
}


/*

  イミディエイトデータのセット

  */
private void
setIMD (disasm* code, operand* op, address ptr, opesize size)
{
    char* optr = op->operand;
    int byte4 = (int) BYTE4;

    op->ea = IMMED;
    op->eaadrs = PC + code->bytes;

    switch (size) {
    case BYTESIZE:
	code->bytes += 2;
	IfNeedStr {
	    *optr++ = '#';
	    if (BYTE3 == 0xff && SignBYTE4 < 0) {
		*optr++ = '-';
		byte4 = -byte4 & 0xff;
	    }
	    itox2d (optr, byte4);
	}
	if (BYTE3 && (BYTE3 != 0xff || SignBYTE4 >= 0) && Disasm_Exact) {
	    UNDEFINED(); return;
	}
	op->opval = (address)(ULONG) BYTE4;
	break;
    case WORDSIZE:
	code->bytes += 2;
	op->opval = (address)(ULONG) WORD2;
	IfNeedStr {
	    *optr++ = '#';
	    itox4d (optr, (LONG) op->opval);
	}
	break;
    case LONGSIZE:
	code->bytes += 4;
	op->opval = (address) peekl (ptr + 2);
	IfNeedStr {
	    *optr++ = '#';
	    itox8d (optr, (LONG) op->opval);
	}
	break;
    default:	/* reduce warning message */
	break;
    }
}


private void
setAnDisp (disasm* code, operand* op, int regno, int disp)
{
    WORD d16 = (WORD) disp;

#ifdef	OSKDIS

    if (regno == 6) {
	ULONG a6disp = (ULONG) disp;
	lblbuf* lptr;
	lblmode wkmode;

	switch (HeadOSK.type & 0x0F00) {
	    case 0x0100:	/* Program */
	    case 0x0b00:	/* Trap */
		a6disp += 0x8000;	/* A6 のオフセットを調整 */
		d16 = disp + 0x8000;	/* A6 のオフセットを調整 */
#if 0
		regist_label(BeginBSS + a6disp, DATLABEL | UNKNOWN);
#endif
		if ((lptr = search_label(BeginBSS + a6disp)) != NULL)
		    wkmode = (lptr->mode & (CODEPTR | DATAPTR));
		else
		    wkmode = DATLABEL;
		regist_label(BeginBSS + a6disp, wkmode | code->size2 | FORCE);
		op->eaadrs = PC + code->bytes;
		op->ea = AregDISP;
		code->bytes += 2;
		IfNeedStr {
		    char* p = op->operand;

		    *p++ = '(';
		    *p++ = 'L';		/* ラベル生成手抜き版	  */
		    *p++ = '0';		/* L00xxxx なラベルを作る */
		    *p++ = '0';		/* 値は７行上で登録済	  */
		    p = strcpy (itox4 (p, BeginBSS + d16), ",a0)");
		    p[2] += regno;
		}
		return;
	    default:
		break;
	}
    }
#endif	/* OSKDIS */

    op->eaadrs = PC + code->bytes;
    op->ea = AregDISP;
    code->bytes += 2;
    IfNeedStr {
	char* p = op->operand;

	*p++ = '(';
	if (d16 < 0) {
	    *p++ = '-';
	    d16 = -d16 & 0xffff;
	}
	p = itox4d (p, d16);
	if (d16 == 0) {
	    *p++ = '.';
	    *p++ = 'w';
	}
	strcpy (p, ",a0)");
	p[2] += regno;
    }
}


/*

  命令の実効アドレス部を解読し、対応する文字列を返す
  code->bytes, code->size をセットしてから呼び出すこと
  ptr にオペコードのアドレス

*/
#define ODDCHECK \
	if (Disasm_AddressErrorUndefined && (long)op->opval & 1 && \
	    code->size2 != BYTESIZE && code->size2 != UNKNOWN) { \
		UNDEFINED(); return; \
	}

private void
setEA (disasm* code, operand* op, address ptr, int mode)
{
    int eamode, eareg;		/* 実効アドレスモード、実効アドレスレジスタ */
    LONG temp;
    char* p = op->operand;

    if (mode & MOVEOPT) {		/* move 命令の第２オペランドの場合 */
	eamode = (WORD1 >> 6) & 7;
	eareg  = (BYTE1 >> 1) & 7;
    }
    else {
	eamode = (WORD1 >> 3) & 7;
	eareg  =  WORD1       & 7;
    }

    if (eamode == 7) {
	WORD d16;

	if (((mode >> 8) & pow2[ eareg ]) == 0
	 && (!(mode & SRCCR) || ((mode & SRCCR) && eareg != 4))) {
	    UNDEFINED(); return;
	}
	op->eaadrs = PC + code->bytes;
	op->ea = 8 + eareg;

	switch (eareg) {
	case 0:				/* (abs).w AbShort */
	    d16 = (WORD) peekw (ptr + code->bytes);
	    op->opval = (address)(LONG) d16;
	    ODDCHECK;
	    IfNeedStr {
		*p++ = '(';
		if (d16 < 0) {
		    *p++ = '-';
		    d16 = -d16 & 0xffff;
		}
		p = itox4d (p, d16);
		*p++ = ')';
#if 0
		*p++ = '.';
		*p++ = 'w';
#endif
		*p++ = '\0';
	    }
	    code->bytes += 2;
	    return;
	case 1:				/* (abs).l AbLong */
	    op->opval = (address) peekl (ptr + code->bytes);
	    ODDCHECK;
	    IfNeedStr {
		*p++ = '(';
		p = itox8d (p, (LONG)op->opval);
		*p++ = ')';
		if ((LONG)(WORD)(LONG) op->opval == (LONG) op->opval) {
		    *p++ = '.';
		    *p++ = 'l';
		}
		*p = '\0';
	    }
	    code->bytes += 4;
	    return;
	case 2:				/* (d16,pc) */
	    op->labelchange1 = TRUE;
	    d16 = (WORD) peekw (ptr + code->bytes);
	    op->opval = PC + code->bytes + d16;
	    ODDCHECK;
	    IfNeedStr {
		*p++ = '(';
		if (d16 < 0) {
		    *p++ = '-';
		    d16 = -d16 & 0xffff;
		}
		itox4d (p, d16);
		strcat (p, ",pc)");
	    }
	    code->bytes += 2;
	    return;
	case 3:				/* (d8,pc,ix) */
	    op->labelchange1 = TRUE;
	    temp = peekw (ptr + code->bytes);	/* temp = 拡張ワード */

	    /* フルフォーマットの拡張ワード */
	    if (temp & 0x0100) {
		boolean zreg = FALSE;
		int indirect = 0, zareg;
		int bdsize = 0, bd = 0;
		int odsize = 0, od = 0;

		if (temp & 0x08) {
		    UNDEFINED(); return;
		}
		REJECT (M000|M010);

		switch ((temp >> 4) & 3) {
		case 0:
		    UNDEFINED(); return;
		case 1:
		    /* bdsize = 0; */
		    break;
		case 2:
		    bdsize = 2;
		    bd = (WORD) peekw (ptr + code->bytes + 2);
		    op->eaadrs = PC + code->bytes + 2;
		    break;
		case 3:
		    bdsize = 4;
		    bd = (LONG) peekl (ptr + code->bytes + 2);
		    op->eaadrs = PC + code->bytes + 2;
		    break;
		}

		switch (((temp & 0x40) >> 3) | (temp & 7)) {
		case 0x0:
		    op->ea = PCIDXB;
		    break;
		case 0x1:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    break;
		case 0x2:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x3:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x5:
		    op->ea = PCPOSTIDX;
		    indirect = 1;
		    break;
		case 0x6:
		    op->ea = PCPOSTIDX;
		    indirect = 1;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x7:
		    op->ea = PCPOSTIDX;
		    indirect = 1;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x8:
		    op->ea = PCIDXB;
		    zreg = TRUE;
		    break;
		case 0x9:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    break;
		case 0xa:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0xb:
		    op->ea = PCPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x4:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
		    UNDEFINED(); return;
		}

		if (zreg) {
			/* サプレスされたレジスタNo が 0 じゃない */
		    if (((UndefRegLevel & 2) && (temp & 0x7000))
			/* サプレスされたレジスタがスケーリングされてる */
		    || ((UndefRegLevel & 4) && (temp & 0x0600))
				/* サプレスされたレジスタが .l 指定されてる */
		    || ((UndefRegLevel & 8) && (temp & 0x0800))
		   ) {
			UNDEFINED(); return;
		    }
		}

		zareg = (temp & 0x80);
		if (bdsize)
		    op->opval = zareg ? (address) bd : PC + bd + code->bytes;
		if (odsize == 4)
		    op->opval2 = (address) od;
		op->exbd = bdsize;
		op->exod = odsize;

		IfNeedStr {
		    *p++ = '(';
		    if (indirect)
			*p++ = '[';

		    if (bdsize) {
			int lim = (bdsize == 2) ? 127 : 32767;
			if (bd < 0) {
			    *p++ = '-';
			    bd = -bd;
			    lim++;
			}

			if (bdsize == 2) {
			    bd &= 0xffff;
			    p = itox4d (p, bd);
			    if (bd <= lim && !indirect) {
				*p++ = '.';		/* (d8.w,pc,ix) */
				*p++ = 'w';
			    }

			} else if (bdsize == 4) {
			    p = itox8d (p, bd);
			    if (bd <= lim) {
				*p++ = '.';	/* (d16.l,pc,ix) */
				*p++ = 'l';
			    }
			}
			*p++ = ',';
		    }

		    if (zareg)
			*p++ = 'z';	/* zpc は省略不可 */
		    *p++ = 'p';
		    *p++ = 'c';

		    if (indirect == 1)
			*p++ = ']';

		    if (!zreg || (UndefRegLevel & 0x0e) != 0x0e) {
			int scale;

			*p++ = ',';
			if (zreg)
			    *p++ = 'z';
			*p++ = temp & 0x8000 ? 'a' : 'd';
			*p++ = ((temp >> 12) & 7) + '0';
			*p++ = '.';
			*p++ = temp & 0x0800 ? 'l' : 'w';

			if ((scale = (temp >> 9) & 3)) {
			    *p++ = '*';
			    *p++ = (scale == 1) ? '2'
				 : (scale == 2) ? '4' : '8';
			}
		    }

		    if (indirect == 2)
			*p++ = ']';

		    if (odsize) {
			int lim = 32767;
			*p++ = ',';
			if (od < 0) {
			    *p++ = '-';
			    od = -od;
			    lim++;
			}

			if (odsize == 2) {
			    od &= 0xffff;
			    p = itox4d (p, od);
			}
			else if (odsize == 4) {
			    p = itox8d (p, od);
			    if (od <= lim) {
				*p++ = '.';
				*p++ = 'l';
			    }
			}
		    }
		    *p++ = ')';
		    *p = '\0';
		}
		code->bytes += 2 + bdsize + odsize;
		return;
	    }

	    /* 短縮フォーマットの拡張ワード */
	    else {
		signed char d8 = *(signed char*) (ptr + code->bytes + 1);

		temp = *(UBYTE*) (ptr + code->bytes);	/* temp = 拡張ワード */
		op->opval = PC + d8 + code->bytes;

		/* scaling check (68020 未満のチェック) */
		if ((temp >> 1) & 0x03)
		    REJECT (M000|M010);

		IfNeedStr {
		    int scale;

		    *p++ = '(';
		    if (d8 < 0) {
			*p++ = '-';
			d8 = -d8 & 0xff;
		    }
		    p = itox2d (p, d8);
		    strcpy (p, ",pc,");
		    p += 4;
		    *p++ = temp & 0x80 ? 'a' : 'd';
		    *p++ = ((temp >> 4) & 7) + '0';
		    *p++ = '.';
		    *p++ = temp & 0x08 ? 'l' : 'w';

		    /* scaling (68020 未満のチェック済) */
		    if ((scale = (temp >> 1) & 3)) {
			*p++ = '*';
			*p++ = (scale == 1) ? '2'
			     : (scale == 2) ? '4' : '8';
		    }
		    *p++ = ')';
		    *p = '\0';
		}
		code->bytes += 2;
		return;
	    }

	case 4:
	    if (mode & SRCCR) {
		op->eaadrs = (address)-1;
		op->ea = SRCCR;

		if (code->size == BYTESIZE || code->size == WORDSIZE) {
		    IfNeedStr {
			strcpy (op->operand, (code->size == BYTESIZE) ? "ccr" : "sr");
		    }
		} else {
		    UNDEFINED(); return;
		}
		return;
	    } else {
		switch (code->size) {		/* #Imm */
		case BYTESIZE:
		    {
			UBYTE undefbyte = *(UBYTE*) (ptr + code->bytes);
			op->opval = (address)(ULONG)*(UBYTE*) (ptr + code->bytes + 1);
			IfNeedStr {
			    temp = (LONG)op->opval;
			    *p++ = '#';
			    if (undefbyte == 0xff && (signed char)(int) op->opval < 0) {
				*p++ = '-';
				temp = -(UBYTE)temp & 0xff;
			    }
			    itox2d (p, temp);
		        }
			if (Disasm_Exact && undefbyte
			 && (undefbyte != 0xff || (signed char)(int) op->opval >= 0)) {
			    UNDEFINED(); return;
			}
			code->bytes += 2;
			return;
		    }
		case WORDSIZE:
		    op->opval = (address)(ULONG) peekw (ptr + code->bytes);
		    IfNeedStr {
			*p++ = '#';
			itox4d (p, (LONG)op->opval);
		    }
		    code->bytes += 2;
		    return;
		case LONGSIZE:
		    op->opval = (address) peekl (ptr + code->bytes);
		    IfNeedStr {
			*p++ = '#';
			itox8d (p, (LONG) op->opval);
		    }
		    code->bytes += 4;
		    return;
		case QUADSIZE:			/* 8bytes (MMU命令専用) */
		    IfNeedStr {
			*p++ = '#';
			fpconv_q (p, (quadword*) (ptr + code->bytes));
		    }
		    code->bytes += 8;
		    return;
		case SINGLESIZE:		/* 単精度実数(4byte) */
		    IfNeedStr {
			*p++ = '#';
			fpconv_s (p, (float*) (ptr + code->bytes));
		    }
		    code->bytes += 4;
		    return;
		case DOUBLESIZE:		/* 倍精度実数(8byte) */
		    IfNeedStr {
			*p++ = '#';
			fpconv_d (p, (double*) (ptr + code->bytes));
		    }
		    code->bytes += 8;
		    return;
		case EXTENDSIZE:		/* 拡張精度実数(12byte) */
		    REJECTnoFPSP ();
		    IfNeedStr {
			*p++ = '#';
			fpconv_x (p, (long double*) (ptr + code->bytes));
		    }
		    code->bytes += 12;
		    return;
		case PACKEDSIZE:		/* パックドデシマル(12byte) */
		    REJECTnoFPSP ();
		    IfNeedStr {
			*p++ = '#';
			fpconv_p (p, (packed_decimal*) (ptr + code->bytes));
		    }
		    code->bytes += 12;
		    return;

		default:	/* reduce warning message */
		    break;
		}
	    }
	}
    }

    else {	/* eamode != 7 */
	if ((mode & pow2[eamode]) == 0) {
	    UNDEFINED(); return;
	}
	op->eaadrs = (address) -1;
	op->ea = eamode;

	if (eamode <= 4 && !Disasm_String)
	    return;

	/* 0～ 4 では 常にオペランド文字列を生成してよい. */
	switch (eamode) {
	case 0:
	    op->operand[0] = 'd';			/* Dn */
	    op->operand[1] = (eareg & 7) + '0';
	    op->operand[2] = 0;
	    return;
	case 1:
	    op->operand[0] = 'a';			/* An */
	    op->operand[1] = (eareg & 7) + '0';
	    op->operand[2] = 0;
	    return;
	case 2:
	    strcpy (op->operand, "(a0)");		/* (An) */
	    op->operand[2] += eareg;
	    return;
	case 3:
	    strcpy (op->operand, "(a0)+");		/* (An)+ */
	    op->operand[2] += eareg;
	    return;
	case 4:
	    strcpy (op->operand, "-(a0)");		/* -(An) */
	    op->operand[3] += eareg;
	    return;
	case 5:						/* (d16,An) */
	    setAnDisp (code, op, eareg, (WORD) peekw (ptr + code->bytes));
	    return;

	case 6:
	    temp = peekw (ptr + code->bytes);		/* temp = 拡張ワード */

	    /* フルフォーマットの拡張ワード */
	    if (*(UBYTE*) (ptr + code->bytes) & 1) {
		boolean zreg = FALSE;
		int indirect = 0, zareg;
		int bdsize = 0, bd = 0;
		int odsize = 0, od = 0;

		if (temp & 0x08) {
		    UNDEFINED(); return;
		}
		REJECT (M000|M010);

		switch ((temp >> 4) & 3) {
		case 0:
		    UNDEFINED(); return;
		case 1:
		    /* bdsize = 0; */
		    break;
		case 2:
		    bdsize = 2;
		    bd = (WORD) peekw (ptr + code->bytes + 2);
		    op->eaadrs = PC + code->bytes + 2;
		    break;
		case 3:
		    bdsize = 4;
		    op->labelchange1 = TRUE;
		    bd = (LONG) peekl (ptr + code->bytes + 2);
		    op->eaadrs = PC + code->bytes + 2;
		    break;
		}

		switch (((temp & 0x40) >> 3) | (temp & 7)) {
		case 0x0:
		    op->ea = AregIDXB;
		    break;
		case 0x1:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    break;
		case 0x2:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x3:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x5:
		    op->ea = AregPOSTIDX;
		    indirect = 1;
		    break;
		case 0x6:
		    op->ea = AregPOSTIDX;
		    indirect = 1;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x7:
		    op->ea = AregPOSTIDX;
		    indirect = 1;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x8:
		    op->ea = AregIDXB;
		    zreg = TRUE;
		    break;
		case 0x9:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    break;
		case 0xa:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    odsize = 2;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (WORD) peekw (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0xb:
		    op->ea = AregPREIDX;
		    indirect = 2;
		    zreg = TRUE;
		    odsize = 4;
		    op->labelchange2 = TRUE;
		    op->eaadrs2 = PC + code->bytes + 2 + bdsize;
		    od = (LONG) peekl (ptr + code->bytes + 2 + bdsize);
		    break;
		case 0x4:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
		    UNDEFINED(); return;
		}

		if (zreg) {
			/* サプレスされたレジスタNo が 0 じゃない */
		    if  (((UndefRegLevel & 2) && (temp & 0x7000))
			/* サプレスされたレジスタがスケーリングされてる */
		      || ((UndefRegLevel & 4) && (temp & 0x0600))
			/* サプレスされたレジスタが .l 指定されてる */
		      || ((UndefRegLevel & 8) && (temp & 0x0800))
		   ) {
			UNDEFINED(); return;
		    }
		}

		zareg = (temp & 0x80);
		if (zareg && (UndefRegLevel & 2) && eareg) {
		    UNDEFINED(); return;	/* サプレスされたレジスタが a0 ではない */
		}

		if (bdsize == 4)
		    op->opval  = (address) bd;
		if (odsize == 4)
		    op->opval2 = (address) od;
		op->exbd = bdsize;
		op->exod = odsize;

		IfNeedStr {
		    *p++ = '(';
		    if (indirect)
			*p++ = '[';

		    if (bdsize) {
			int lim = (bdsize == 2) ? 127 : 32767;
			if (bd < 0) {
			    *p++ = '-';
			    bd = -bd;
			    lim++;
			}

			if (bdsize == 2) {
			    bd &= 0xffff;
			    p = itox4d (p, bd);
			    if (bd <= lim && !indirect) {
				*p++ = '.';	/* (d8.w,an,ix) */
				*p++ = 'w';
			    }

			} else if (bdsize == 4) {
			    p = itox8d (p, bd);
			    if (bd <= lim) {
				*p++ = '.';	/* (d16.l,an,ix) */
				*p++ = 'l';
			    }
			}
		    }

		    if (!zareg || (UndefRegLevel & 2) == 0) {
			if (bdsize)
			    *p++ = ',';
			if (zareg)
			    *p++ = 'z';
			*p++ = 'a';
			*p++ = eareg + '0';
		    }

		    if (indirect == 1)
			*p++ = ']';

		    if (!zreg || (UndefRegLevel & 0x0e) != 0x0e) {
			int scale;

			if (p != &op->operand[1])
			    *p++ = ',';
			if (zreg)
			    *p++ = 'z';
			*p++ = (temp & 0x8000 ? 'a' : 'd');
			*p++ = ((temp >> 12) & 7) + '0';
			*p++ = '.';
			*p++ = (temp & 0x0800 ? 'l' : 'w');
			if ((scale = (temp >> 9) & 3)) {
			    *p++ = '*';
			    *p++ = (scale == 1) ? '2'
				 : (scale == 2) ? '4' : '8';
			}
		    }

		    if (indirect == 2)
			*p++ = ']';

		    if (odsize) {
			int lim = 32767;
			*p++ = ',';
			if (od < 0) {
			    *p++ = '-';
			    od = -od;
			    lim++;
			}

			if (odsize == 2) {
			    od &= 0xffff;
			    p = itox4d (p, od);
			} else if (odsize == 4) {
			    p = itox8d (p, od);
			    if (bd < lim) {
				*p++ = '.';
				*p++ = 'l';
			    }
			}
		    }
		    *p++ = ')';
		    *p = '\0';
		}
		code->bytes += 2 + bdsize + odsize;
		return;
	    }

	    /* 短縮フォーマットの拡張ワード */
	    else {					/* (d8,An,ix) */
		signed char d8 = *(signed char*) (ptr + code->bytes + 1);

		temp = *(UBYTE*) (ptr + code->bytes);
		op->eaadrs = PC + code->bytes;

		/* scaling check (68020 未満のチェック) */
		if ((temp >> 1) & 3)
		    REJECT (M000|M010);

		IfNeedStr {
		    int scale;

		    *p++ = '(';
		    if (d8) {
			if (d8 < 0) {
			    *p++ = '-';
			    d8 = -d8 & 0xff;
			}
			p = itox2d (p, d8);
			*p++ = ',';
		    }
		    *p++ = 'a';
		    *p++ = eareg + '0';
		    *p++ = ',';
		    *p++ = (temp & 0x80 ? 'a' : 'd');
		    *p++ = ((temp >> 4) & 7) + '0';
		    *p++ = '.';
		    *p++ = (temp & 0x08 ? 'l' : 'w');

		    /* scaling (68020 未満のチェック済) */
		    if ((scale = (temp >> 1) & 3)) {
			*p++ = '*';
			*p++ = (scale == 1) ? '2'
			     : (scale == 2) ? '4' : '8';
		    }
		    *p++ = ')';
		    *p = '\0';
		}
		code->bytes += 2;
		return;
	    }
	}
    }
}

/* EOF */
