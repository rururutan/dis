/* $Id: main.c,v 2.76 1995/01/07 11:32:22 ryo Exp $
 *
 *	ソースコードジェネレータ
 *	メインルーチン
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */


#include <ctype.h>	/* tolower */
#include <limits.h>	/* PATH_MAX */
#include <stdio.h>
#include <stdlib.h>	/* exit */
#include <string.h>	/* memcmp */
#include <time.h>	/* time difftime */
#include <unistd.h>	/* access */
#include <sys/stat.h>
#include <sys/fcntl.h>	/* open */
#include <sys/types.h>

#ifdef QUICK_YES_NO
#ifdef __HUMAN68K__
#include <conio.h>	/* getch */
#else
#include <curses.h>
#endif
#endif	/* QUICK_YES_NO */

#ifndef O_BINARY
#define O_BINARY 0
#endif


#include "analyze.h"
#include "disasm.h"
#include "estruct.h"
#include "etc.h"
#include "generate.h"
#include "global.h"
#include "include.h"	/* load_OS_sym() */
#include "label.h"
#include "labelfile.h"
#include "offset.h"
#include "option.h"
#include "symbol.h"
#include "table.h"


USEOPTION	option_g, option_e, option_v, option_d, option_i,
		option_p, option_l, option_p, option_c, option_z,
		option_D, option_T;
USEOPTION	option_overwrite;


/* static 関数プロトタイプ */
private void	analyze_and_generate (int, char*[]);
private void	check_open_output_file (char*);
private int	check_exist_output_file (char*);
private void	change_filename (char** nameptr, const char* file);


xheader Head;
#ifdef	OSKDIS
os9header HeadOSK;
#else
char	FileType;	/* ファイル形式(0,'x','r','z') */
#endif

ULONG	Top;		/* 実際にファイルのあるアドレス  */
ULONG	Ofst;		/* 実際のアドレス - 仮想アドレス */

/********************************************
  Human68K 用（-DOSKDIS 無）の場合は、
    BeginTEXT = text section の先頭
    BeginDATA = data section の先頭
    BeginBSS  = bss section の先頭
  OS-9/680x0 用（-DOSKDIS 有）の場合は、
    BeginTEXT = psect 先頭
    BeginDATA = 初期化 vsect 先頭
    BeginBSS  = vsect 先頭
*********************************************/

address BeginTEXT,
	BeginDATA,
	BeginBSS,
#ifndef OSKDIS
	BeginSTACK,
#endif
	Last;
address Available_text_end;	/* = BeginDATA or BeginBSS */

int Absolute  = NOT_ABSOLUTE;	/* 絶対番地形式モード */
				/* NOT_ABSOLUTE, ABSOLUTE_ZFILE, ABSOLUTE_ZOPT */

int String_length_min = 3;  /* 文字列の最小長さ */

int	Debug = 0;
int	Verbose = 1;
int	Quiet_mode = 0;
short	Emulate_mode = 3;	/* bit0=1:fpsp bit1=1:isp emulation 命令を認識する */
char	CommentChar = ';';

address Base;		/* ロードアドレス */
address Exec;		/* 実行開始アドレス */
boolean Exist_symbol;	/* シンボルテーブルの存在フラグ */

char*	Filename_in;
char*	Filename_out;
char*	Labelfilename_in;
char*	Labelfilename_out;
char*	Tablefilename;


extern void
print_title (void)
{
    static char flag = 1;

    if (flag) {
	flag = 0;

	eprintf ("ソースコードジェネレータ for X680x0"
#ifdef __linux__
						" (Linux cross)"
#endif
#ifdef __FreeBSD__
						" (BSD cross)"
#endif
#ifdef __CYGWIN__
						" (Cygwin cross)"
#endif
#ifdef __MINGW32__
						" (MinGW cross)"
#endif
							" version %s\n"
		 "Copyright (C)1989-1992 K.Abe, 1994-1997 R.ShimiZu, "
				"%s Tachibana.\n", Version, Date);

#ifdef	OSKDIS
	eprintf ("OS-9/68000 version %s by TEMPLE, 1994\n", OSKEdition);
#endif
    }
}


typedef struct {
    short bit;
    char* name;
} target_table;

private void
print_target (char* str, int bits, const target_table* tbl, int size)
{
    if (bits) {
	int f = '\0';
	eprintf (str);
	do {
	    if (bits & tbl->bit) {
		if (f)
		    eputc (f);
		eprintf (tbl->name);
		f = ',';
	    }
	    tbl++;
	} while (--size);
	eputc ('\n');
    }
}


/*

  ファイルを読み込み、ヘッダ情報を大域変数 Head にセット
  ファイルの日付を Filedate にセット
  読み込んだ先頭アドレスを返す

*/
static time_t	Filedate;

static INLINE address
loadfile (char* filename)
{
    int		handle;
    address	top;
    ULONG	bytes;
    struct stat	statbuf;

    if ((handle = open (filename, O_BINARY | O_RDONLY)) == -1)
	err ("%s をオープン出来ません.\n", filename);

    if (fstat( handle , &statbuf) < 0)
	err ("fstat failed.\n");
    Filedate = statbuf.st_mtime;

#ifdef	OSKDIS
    if (option_z) {
	Head.base = Base;
	Head.exec = Exec;
	Head.text = statbuf.st_size;
	Head.data = 0;
	Head.bss = 0;
	Head.symbol = 0;
	Head.bindinfo = 0;
    }
    else {
	if (read (handle, (char *)&HeadOSK, sizeof (HeadOSK)) != sizeof (HeadOSK))
	    goto os9head_error;
	if (HeadOSK.head != 0x4afc) {
	    eprintf ("ID=%04X\n", HeadOSK.head);
os9head_error:
	    close(handle);
	    err ("OS-9/680x0 のモジュールではありません.\n");
	}
	switch (HeadOSK.type & 0x0f00) {
	    case 0x0100:	/* Program */
		Head.base = 0x0048;
		Head.bss = HeadOSK.mem;
		break;
	    case 0x0b00:	/* Trap */
		Head.base = 0x0048;
		Head.bss = HeadOSK.mem;
		break;
	    case 0x0200:	/* Subroutine */
	    case 0x0c00:	/* System     */
	    case 0x0d00:	/* FileMan    */
	    case 0x0e00:	/* Driver     */
		Head.base = 0x003c;
		Head.bss = HeadOSK.mem;
		break;
	    default:
		close(handle);
		err ("このモジュールタイプはサポートしていません.\n");
	}
	lseek(handle, Head.base, 0);
	Head.exec = HeadOSK.exec;
	Head.text = statbuf.st_size - (ULONG)(Head.base);
	Head.data = 0;
	Head.symbol = 0;
	Head.bindinfo = 0;
    }
#else
    if (FileType == 0) {
	char*	ext = strrchr (filename, '.');

	/* R 形式は拡張子で判別する */
	if (ext && strcasecmp (ext, ".r") == 0)
	    FileType = 'r';
	else {
	    char buf[2];

	    /* 先頭 2 バイトを読み込む */
	    if (read (handle, buf, 2) < 2)
		goto filetype_error;		/* 2 バイト未満なら実行ファイルではない */
	    lseek (handle, 0, SEEK_SET);

	    /* ファイル形式を判別する */
	    if (buf[0] == (char)'H' && buf[1] == (char)'U')
		FileType = 'x';
	    else if (buf[0] == (char)0x60 && buf[1] == (char)0x1a)
		FileType = 'z';
	    else
		goto filetype_error;
	}
    }

    if (option_z || FileType == (char)'r') {
	Head.base = Base;
	Head.exec = Exec;
	Head.text = statbuf.st_size;
	Head.data = 0;
	Head.bss = 0;
	Head.symbol = 0;
	Head.bindinfo = 0;
    } else if (FileType == (char)'z') {
	zheader zhead;

	if (read (handle, (char*) &zhead, sizeof (zhead)) != sizeof (zhead))
	    goto filetype_error;
	Head.exec = Head.base = (address) peekl (&zhead.base);
	Head.text = peekl (&zhead.text);
	Head.data = peekl (&zhead.data);
	Head.bss  = peekl (&zhead.bss);
	Head.symbol = 0;
	Head.bindinfo = 0;
	Absolute = ABSOLUTE_ZFILE;
    } else {		/* if (FileType == (char)'x') */
	if (read (handle, (char *)&Head, sizeof (Head)) != sizeof (Head)) {
filetype_error:
	    close (handle);
	    err ("このようなファイルは取り扱っておりません.\n");
	}

#ifndef __BIG_ENDIAN__
	/* big-endian で格納されているヘッダを little-endian に変更する */
	Head.base = (address) peekl (&Head.base);
	Head.exec = (address) peekl (&Head.exec);
	Head.text = peekl (&Head.text);
	Head.data = peekl (&Head.data);
	Head.bss  = peekl (&Head.bss);
	Head.offset = peekl (&Head.offset);
	Head.symbol = peekl (&Head.symbol);
#endif

    }
#endif	/* OSKDIS */

    bytes = Head.text + Head.data + Head.offset + Head.symbol;
    top = (address) Malloc (bytes + 16);

    if (read (handle, (char*)top, bytes) != bytes) {
	close (handle);
	err ("ファイルサイズが異常です.\n");
    }

    close (handle);
    return top;
}

/*

  後始末

*/
extern void
free_load_buffer (void)
{
    Mfree ((void *) Top);
}


int
main (int argc, char* argv[])
{
    time_t start_time = time (NULL);

#ifdef	OSK
    setbuf (stdout, NULL);
#endif
    setbuf (stderr, NULL);

    analyze_args (argc, argv);
    print_title ();

    eprintf ("%s を読み込みます.\n", Filename_in);

    /* アドレス関係の大域変数を初期化 */
    Top = (ULONG)loadfile (Filename_in);
    Ofst = Top - (ULONG)Head.base;
    BeginTEXT = Head.base;

#ifdef	OSKDIS
    BeginBSS  = BeginTEXT + Head.text;
    BeginDATA = BeginBSS  + Head.bss;
    Last      = BeginDATA + Head.data;
    Available_text_end = BeginBSS;

#else
    BeginDATA = BeginTEXT + Head.text;
    BeginBSS  = BeginDATA + Head.data;
    Last = BeginSTACK = BeginBSS  + Head.bss;
    Available_text_end = (option_D ? BeginBSS : BeginDATA);

    if (Head.bindinfo) {
	eprintf ("このファイルは bind されています.\n"
		 "unbind コマンド等で展開してからソースジェネレートして下さい.\n");
	return 1;
    }
#endif	/* OSKDIS */


    load_OS_sym();
    init_labelbuf();
    init_symtable();


    /* 対象 MPU/MMU/FPU の表示 */
    {
	static const target_table mpu_table[] = {
	    { M000, "68000" },
	    { M010, "68010" },
	    { M020, "68020" },
	    { M030, "68030" },
	    { M040, "68040" },
	    { M060, "68060" },
	    { MISP, "060ISP" }
	};

	static const target_table mmu_table[] = {
	    { MMU851, "68851" },
	    { MMU030, "68030" },
	    { MMU040, "68040" },
	    { MMU060, "68060" }
	};

	static const target_table fpu_table[] = {
	    { F881, "68881" },
	    { F882, "68882" },
	    { F040, "68040" },
	    { F4SP, "040FPSP" },
	    { F060, "68060" },
	    { F6SP, "060FPSP" }
	};

	print_target ("Target MPU: ", MPU_types,
		mpu_table, (sizeof mpu_table / sizeof mpu_table[0]));

	print_target ("Target MMU: ", MMU_type,
		mmu_table, (sizeof mmu_table / sizeof mmu_table[0]));

	print_target ("Target FPU: ", FPCP_type,
		fpu_table, (sizeof fpu_table / sizeof fpu_table[0]));
    }


#ifndef OSKDIS
    eprintf ("リロケート情報を展開します.\n");
    make_relocate_table ();
#endif	/* !OSKDIS */


    while (check_exist_output_file (Filename_out))
	change_filename (&Filename_out, "source");

    if (option_e)
	while (check_exist_output_file (Labelfilename_out))
	    change_filename (&Labelfilename_out, "label");

    check_open_output_file (Filename_out);

    if (option_g) {
	eprintf ("ラベルファイルを読み込み中です.\n");
	read_labelfile (Labelfilename_in);
    }
    if (option_e)
	check_open_output_file (Labelfilename_out);
    if (option_T) {
	eprintf ("テーブル記述ファイルを読み込み中です.\n");
	read_tablefile (Tablefilename);
    }

#ifndef OSKDIS
    if (Head.symbol) {
	eprintf ("シンボルテーブルを展開します.\n");
	make_symtable ();
    } else
	eprintf ("シンボルテーブルは残念ながら存在しません.\n");
#endif

    Exist_symbol = is_exist_symbol ();

    analyze_and_generate (argc, argv);

    eprintf ("\n終了しました.\n");
    free_labelbuf ();
    free_relocate_buffer ();
    free_symbuf ();
    free_load_buffer ();

    {
	time_t finish_time = time (NULL);
	eprintf ("所要時間: %d秒\n", (int) difftime (finish_time, start_time));
    }

    return 0;
}



/*

  デバイスドライバの処理

*/
#ifndef OSKDIS
private void
analyze_device (void)
{
    ULONG ad = 0;

    Reason_verbose = (Verbose >= 1) ? TRUE : FALSE;

    do {
	regist_label (Head.base + ad       , DATLABEL | LONGSIZE | FORCE);
	regist_label (Head.base + ad + 4   , DATLABEL | WORDSIZE | FORCE | HIDDEN);
#if 0
	regist_label (Head.base + ad + 6   , DATLABEL | LONGSIZE | FORCE);
	regist_label (Head.base + ad + 0xa , DATLABEL | LONGSIZE | FORCE);
#endif
	regist_label (Head.base + ad + 0xe , DATLABEL | STRING | FORCE | HIDDEN);
	regist_label (Head.base + ad + 0x16, DATLABEL | UNKNOWN);

	analyze (*(address*) (Ofst + ad + 6  ), ANALYZE_IGNOREFAULT);
	analyze (*(address*) (Ofst + ad + 0xa), ANALYZE_IGNOREFAULT);
	ad = *(ULONG*) (ad + Ofst);
    } while (((long)ad & 1) == 0
#if 0
		&& (ad != (address)-1)		/* 不要 */
#endif
		&& (ad < (ULONG)BeginBSS));	/* SCSIDRV.SYS 対策 */
}
#endif	/* OSKDIS */


/*

  解析して生成

*/
private void
analyze_and_generate (int argc, char* argv[])
{

    if (option_v) {
	eprintf ("逆アセンブルリストを出力します.\n");
	disasmlist (Filename_in, Filename_out, Filedate);
	return;
    }

    Disasm_String = FALSE;  /* 解析中はニーモニックは不要 */

    if (!option_g) {

#ifdef	OSKDIS
	switch (HeadOSK.type & 0x0f00) {
	    case 0x0100:	/* Program */
	    case 0x0b00:	/* Trap */
		eprintf ("\n初期化データ領域解析中です.");
		analyze_idata ();	/* 初期データオフセットを解析   */
		analyze_irefs ();	/* 初期データ参照テーブルを解析 */
		eputc ('\n');
	    default:
		break;
	}
	regist_label (Head.base + Head.text + Head.data + Head.bss, DATLABEL | UNKNOWN);
#endif	/* OSKDIS */

	eprintf ("プログラム領域解析中です.");

	/* セクションのエントリアドレスをテーブルに登録 */
	regist_label (BeginTEXT, DATLABEL | UNKNOWN);
	regist_label (BeginDATA, DATLABEL | UNKNOWN);
	regist_label (BeginBSS , DATLABEL | UNKNOWN);
#if 0
	regist_label (Last,	 DATLABEL | UNKNOWN);
#else	/* 最後のラベルが unregist される不具合の対処 */
	regist_label (Last,	 DATLABEL | UNKNOWN | SYMLABEL);
#endif
	Reason_verbose = (Verbose >= 1) ? TRUE : FALSE;

#ifdef	OSKDIS
	switch (HeadOSK.type & 0x0f00) {
	    case 0x0b00:	/* Trap */
		regist_label (HeadOSK.init, PROLABEL | WORDSIZE | FORCE);
		regist_label (HeadOSK.term, PROLABEL | WORDSIZE | FORCE);
		z_table(0x000048);
	    case 0x0100:	/* Program */
#if 1	/* デバッグの為のコード */
		regist_label (HeadOSK.idata, DATLABEL | LONGSIZE | FORCE);
		regist_label (HeadOSK.irefs, DATLABEL | WORDSIZE | FORCE);
#endif
		break;
	    case 0x0200:	/* Subroutine */
	    case 0x0c00:	/* System */
		break;
	    case 0x0d00:	/* FileMan */
		regist_label (HeadOSK.exec + 0, DATLABEL | WORDSIZE | FORCE);
		relative_table (HeadOSK.exec);
		break;
	    case 0x0e00:
		regist_label (HeadOSK.exec + 0, DATLABEL | WORDSIZE | FORCE);
		w_table (HeadOSK.exec);
		break;
	}
	regist_label (HeadOSK.excpt, PROLABEL | WORDSIZE | FORCE);
	regist_label (HeadOSK.name, DATLABEL | STRING | FORCE);
#if 1
	regist_label (HeadOSK.size - 4, DATLABEL | BYTESIZE | FORCE);	/* CRC */
#endif
	analyze (Head.exec, ANALYZE_IGNOREFAULT);

#else	/* !OSKDIS */
	if (option_d) {
	    analyze_device ();
	    if (Head.exec != Head.base)
		analyze (Head.exec, option_i ? ANALYZE_IGNOREFAULT : ANALYZE_NORMAL);
	} else
	    analyze (Head.exec, ANALYZE_IGNOREFAULT);
#endif	/* OSKDIS */
    }

    Reason_verbose = (Verbose == 2) ? TRUE : FALSE;
    eprintf ("\nデータ領域解析中です.");
    analyze_data ();

#if 0
    if (!option_g) {
	eprintf ("\nアドレステーブルから捜しています.");
	search_adrs_table ();
    }
#endif

    if (!option_p)
	do
	    eprintf ("\nデータ領域の中からプログラム領域を捜しています(1).");
	while (research_data () && !option_l);

    if (String_length_min)
	do {
	    eprintf ("\n文字列を捜しています.");
	    if (!search_string (String_length_min) || option_p)
		break;
	    eprintf ("\nデータ領域の中からプログラム領域を捜しています(2).");
	    if (!research_data ())
		break;
	    do
		eprintf ("\nデータ領域の中からプログラム領域を捜しています.");
	    while (research_data () && !option_l);
	} while (!option_l);

    if (!option_c) {
	eprintf ("\nラベルチェック中.");
	search_operand_label ();
    }

    if (option_e) {
	eprintf ("ラベルファイルを作成中です.\n");
	make_labelfile (Filename_in, Labelfilename_out);
    }

    eprintf ("ソースを作成中です.");
    generate (Filename_in, Filename_out, Filedate, argc, argv);
}


/*

  出力ファイルの存在チェック

  返値: 0 = 出力可能
	1 = ファイル名を変更する

*/
private int
check_exist_output_file (char* filename)
{
    FILE* fp;

    /* --overwrite 指定時は無条件に上書き. */
    if (option_overwrite)
	return 0;

    /* 標準出力に出力するなら検査不要. */
    if (strcmp ("-", filename) == 0)
	return 0;

    /* ファイルが存在しなければOK. */
    if ((fp = fopen (filename, "r")) == NULL)
	return 0;

    /* 端末デバイスへの出力ならOK. */
    if (isatty (fileno (fp))) {
	fclose (fp);
	return 0;
    }

    /* いずれでもなければキー入力. */
    fclose (fp);

#ifdef	QUICK_YES_NO	/* version 2.79 互換 */
    {
	int key;
	eprintf ("%s が既に存在します.\n"
		 "Over Write ( Y or N )? ", filename);
	do
	    key = toupper (getch ());
	while (key != 'Y' && key != 'N');
	eprintf ("%c\n", key);
	if (key == 'Y')
	    return 0;
    }
#else			/* 安全の為 "yes" を入力させる. */
    {
	char buf[256];
	eprintf ("%s が既に存在します.\n"
		 "上書きしますか？(Yes/No/Rename): ", filename);

	if (fgets (buf, sizeof buf, stdin)) {
	    /* y, yes なら上書き */
	    if (strcasecmp (buf, "y\n") == 0 || strcasecmp (buf, "yes\n") == 0)
		return 0;
	    /* r, rename ならファイル名変更 */
	    if (strcasecmp (buf, "r\n") == 0 || strcasecmp (buf, "rename\n") == 0)
		return 1;
	}
    }
#endif	/* QUICK_YES_NO */

    /* 上書きしないならプログラム終了. */
    exit (1);
}


/*

  出力ファイル名を変更する.

*/
private void
change_filename (char** nameptr, const char* file)
{
    char buf[PATH_MAX + 1];
    int len;

    eprintf ("Input %s filename:", file);
#if 0
    fflush (stderr);
#endif
    if (fgets (buf, sizeof buf, stdin) && (len = strlen (buf)) > 1) {
	buf[len - 1] = '\0';
	*nameptr = strcpy (Malloc (len), buf);
	return;
    }
    exit (1);
}


/*

  出力ファイルに書き込めるどうかのチェック

*/
private void
check_open_output_file (char* filename)
{
    int fd;

    if (!strcmp ("-", filename))
	return;

#ifdef	OSK
    if ((fd = open (filename, S_IREAD)) < 0)
#else
    if ((fd = open (filename, O_RDONLY)) < 0)
#endif	/* OSK */
	return;

    if (isatty (fd)) {
	close (fd);
	return;
    }
    close (fd);
    if (access (filename, R_OK | W_OK) < 0)
	err ("%s にアクセスできません(!)\n", filename);
}

/* EOF */
