/* $Id: option.c,v 1.1 1996/11/07 08:03:54 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	Option analyze , etc
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __LIBC__
#include <sys/xglob.h>
#else
#define _toslash(p) (p)
#endif

#include "analyze.h"		/* Arg_after_call */
#include "disasm.h"
#include "estruct.h"
#include "etc.h"		/* strupr */
#include "fpconv.h"		/* Inreal_flag */
#include "generate.h"
#include "getopt.h"
#include "global.h"
#include "hex.h"
#include "include.h"		/* xxx_mac */
#include "option.h"
#include "output.h"		/* Output_AddressCommentLine, Output_SplitByte */
#include "symbol.h"		/* Output_Symbol */


boolean option_a, option_c, option_d, option_e, option_g, option_h,
	option_i, option_k, option_l, option_p, option_q, option_r,
	option_v, option_x, option_y, option_z, option_B,
	option_D, option_E, option_I, option_M, option_N, /* option_Q, */
	option_S, option_T, option_U, option_Y, option_Z;

boolean option_overwrite;


/* main.c */
extern void	print_title (void);

extern short	Emulate_mode;	/* bit0=1:fpsp bit1=1:isp emulation命令を認識する */
extern int	String_length_min;
extern char	CommentChar;
extern address	Base, Exec;
extern int	Verbose;
extern int	Quiet_mode;
#ifndef	OSKDIS
extern char	FileType;
#endif

extern char*	Filename_in;
extern char*	Filename_out;
extern char*	Labelfilename_in;
extern char*	Labelfilename_out;
extern char*	Tablefilename;


/* static 関数プロトタイプ */
private void	option_switch (int, char**);



/*

  オプションを解析する

*/
extern void
analyze_args (int argc, char* argv[])
{
    int fileind;
    char* envptr;

    static const char optionlist[] =
	"a::b:cde::fg::hijklm:n:o:pq::rs::u::vw:xyz:"
	"ABC::DEFGIK:L:MNP:QR:S::T::UV:W:XYZ::#:-:";

#ifdef	READ_FEFUNC_H
    fefunc_mac = "fefunc.h";
#endif

    /* process environment variable `dis_opt' */
    if (/* option_Q == FALSE && */ (envptr = getenv (DIS_ENVNAME))) {
	int c;
	int count, cnt;
	char *envs, *envp;	/* envs=固定, envp=作業用 */
	char **args, **argp;	/* args=固定, argp=作業用 */

	/* 引数をスペースで分割する */
	while (*envptr == ' ')
	    envptr++;
	envp = envs = Malloc (strlen (envptr) + 1);
	for (count = 1; *envptr; count++) {
	    while (*envptr && (*envptr != ' '))
		*envp++ = *envptr++;
	    *envp++ = '\0';
	    while (*envptr == ' ')
		envptr++;
	}

	/* 各引数へのポインタ配列を作る */
	argp = args = Malloc ((count + 1) * sizeof (char*));
	envp = envs;
	*argp++ = argv[0];		/* my name */
	for (cnt = count; --cnt;) {
	    *argp++ = envp;
	    while (*envp++)
		;
	}
	*argp = NULL;

	/* オプション解析 */
	/* optind = -1; */
	while ((c = dis_getopt (count, args, optionlist)) != EOF)
	    option_switch (c, args);

	/* 忘れずに解放 */
	Mfree (args);
	Mfree (envs);
    }

    /* process commandline variable */
    {
	int c;
	optind = -1;
	while ((c = dis_getopt (argc, argv, optionlist)) != EOF)
	    option_switch (c, argv);
	fileind = optind;
    }


    while (fileind < argc) {
	if (Filename_in == NULL)
	    Filename_in = argv[fileind++];
	else if (Filename_out == NULL)
	    Filename_out = argv[fileind++];
	else
	    err ("ファイル名が多すぎます.\n");
    }
    if ((Filename_in  == NULL) || (*Filename_in  == (char)'\0'))
	usage (1);
    if ((Filename_out == NULL) || (*Filename_out == (char)'\0'))
	Filename_out = "-";


    if ((MMU_type & MMU851) && !(MPU_types & M020))
	err ("-m68851 は -m68020 としか併用できません.\n");

    if ((FPCP_type & F88x) && !(MPU_types & (M020|M030)))
	err ("-m6888x は -m68020/68030 としか併用できません.\n");

    if ((Emulate_mode & 2) && (MPU_types & M060))
	MPU_types |= MISP;

    if (Emulate_mode & 1) {
	short tmp = FPCP_type;
	if (tmp & F040) tmp |= F4SP;
	if (tmp & F060) tmp |= F6SP;
	FPCP_type = tmp;
    }


    /* if invoked with -T , labelfile must be read */
    if (option_T)
	option_g = TRUE;


    /* ラベルファイル/テーブルファイル名を作成する */
    {
	char* file = strcmp ("-", Filename_out) ? Filename_out : "aout";
	char* buf = Malloc (strlen (file) + 1);
	size_t len;

	_toslash (strcpy (buf, file));

	{
	    char* p = strrchr (buf, '/');
	    p = p ? (p + 1) : buf;
	    p = strrchr ((*p == (char)'.') ? (p + 1) : p, '.');
	    if (p)
		*p = '\0';		/* 拡張子を削除する */
	}
	len = strlen (buf) + 4 + 1;

	if (option_g && Labelfilename_in == NULL) {
	    Labelfilename_in = Malloc (len);
	    strcat (strcpy (Labelfilename_in, buf), ".lab");
	}
	if (option_e && Labelfilename_out == NULL) {
	    Labelfilename_out = Malloc (len);
	    strcat (strcpy (Labelfilename_out, buf), ".lab");
	}
	if (option_T && Tablefilename == NULL) {
	    Tablefilename = Malloc (len);
	    strcat (strcpy (Tablefilename, buf), ".tab");
	}

	Mfree (buf);
    }
}


private int
ck_atoi (char* str)
{
    unsigned char c;
    unsigned char* ptr = (unsigned char*) str;

    while ((c = *ptr++) != '\0') {
	if ((c < '0') || ('9' < c))
	    err ("数値の指定が異常です.\n");
    }

    return atoi (str);
}


private void
init_fpuid_table (void)
{
    memset (FPUID_table, 0, sizeof FPUID_table);
}


private const char**
include_option (char* p)
{
    char eq;

    if (strncmp (p, "exclude-", 8) != 0 &&
	strncmp (p, "include-", 8) != 0)
	return NULL;

    eq = (*p == (char)'e') ? '\0' : '=';
    p += 8;

    if (strncmp (p, "doscall-mac", 11) == 0 && p[11] == eq)
	return &doscall_mac;
    if (strncmp (p,"iocscall-mac", 12) == 0 && p[12] == eq)
	return &iocscall_mac;
    if (strncmp (p,  "fefunc-mac", 10) == 0 && p[10] == eq)
	return &fefunc_mac;

    return NULL;
}

private const char*
include_filename (char* p)
{
    size_t len;

    if (*p == (char)'e')
	return NULL;

    while (*p++ != '=')
	;

    len = strlen (p);
    return len ? _toslash (strcpy (Malloc (len + 1), p))
	       : NULL;
}

private void
option_switch (int opt, char** argv)
{
    switch (opt) {
    case 'a':
	option_a = TRUE;
	Output_AddressCommentLine = (optarg ? ck_atoi (optarg) : 5);
	break;
    case 'b':
	Generate_SizeMode = ck_atoi (optarg);
	break;
    case 'c':
	option_c = TRUE;
	break;
    case 'd':
	option_d = TRUE;
	break;
    case 'e':
	option_e = TRUE;
	if (optarg)
	    Labelfilename_out = optarg;
	break;
    case 'f':
	Disasm_Exact = FALSE;
	break;
    case 'g':
	option_g = TRUE;
	if (optarg)
	    Labelfilename_in = optarg;
	break;
    case 'h':
	option_h = TRUE;
	break;
    case 'i':
	option_i = TRUE;
	break;
    case 'j':
	Disasm_AddressErrorUndefined = FALSE;
	break;
    case 'k':
	option_k = TRUE;
	break;
    case 'l':
	option_l = TRUE;
	break;
    case 'm':
	{
	    int init = 0;
	    char* next;
#ifndef	__LIBC__
	    char* new_optarg = (char*) Malloc (strlen (optarg) + 1);
	    optarg = strcpy (new_optarg, optarg);
#endif
	    do {
		int m;

		next = strchr (optarg, ',');
		if (next)
		    *next++ = '\0';

#ifdef	COLDFIRE
		if (strcasecmp (optarg, "cf") == 0
		 || strcasecmp (optarg, "coldfire") == 0) {
		    Disasm_ColdFire = TRUE;
		    continue;
		}
#endif
		if (strcasecmp (optarg, "cpu32") == 0) {
		    Disasm_CPU32 = TRUE;
		    continue;
		}
		if (strcasecmp (optarg, "680x0") == 0)
		    m = -1;
		else {
		    m = ck_atoi (optarg);
		    if (m == 68851) {
			MMU_type |= MMU851;
			continue;
		    } else if (m == 68881 || m == 68882) {
			int id = next ? ck_atoi (next) : 1;

			FPCP_type |= (m == 68882) ? F882 : F881;
			if (id & ~7)
			    err ("FPU IDの値が異常です(-m).\n");
			FPUID_table[id*2] = 1;
			break;
		    }
		}

		/* 680x0, 68000, 68010, ... */
		if (init == 0) {
		    init = 1;
		    MPU_types = 0;
		    FPCP_type = MMU_type = 0;
		    init_fpuid_table ();
		}
		switch (m) {
		case 68000:
		case 68008:
		    MPU_types |= M000;
		    break;
		case 68010:
		    MPU_types |= M010;
		    break;
		case 68020:
		    MPU_types |= M020;
		    break;
		case 68030:
		    MPU_types |= M030;
		    MMU_type |= MMU030;
		    break;
		case 68040:
		    MPU_types |= M040;
		    FPCP_type |= F040;
		    MMU_type |= MMU040;
		    FPUID_table[1*2] = 1;
		    break;
		case 68060:
		    MPU_types |= M060;
		    FPCP_type |= F060;
		    MMU_type |= MMU060;
		    FPUID_table[1*2] = 1;
		    break;
		case -1:	/* -m680x0 */
		    MPU_types |= (M000|M010|M020|M030|M040|M060);
		    FPCP_type |= (F040|F060);
		    MMU_type |= (MMU030|MMU040|MMU060);
		    FPUID_table[1*2] = 1;
		    break;
		default:
		    err ("指定の MPU, FPU はサポートしていません(-m).\n");
		    /* NOT REACHED */
		}
	    } while ((optarg = next) != NULL);
#ifndef	__LIBC__
	    Mfree (new_optarg);
#endif
	}
	break;
    case 'n':
	String_length_min = ck_atoi (optarg);
	break;
    case 'o':
	String_width = ck_atoi (optarg);
	if( String_width < 1 || 80 < String_width )
	    err ("値が無効な範囲です(-o).\n");
	break;
    case 'p':
	option_p = TRUE;
	break;
    case 'q':
	option_q = TRUE;
	if( optarg ) Quiet_mode = ck_atoi (optarg);
	if( Quiet_mode < 0 || 1 < Quiet_mode )
	    err ("値が無効な範囲です(-q).\n");
	break;
    case 'r':
	option_r = TRUE;
	break;
    case 's':
	Output_Symbol = optarg ? ck_atoi (optarg) : 0;
	if ((unsigned short)Output_Symbol > 2)
	    err ("値が無効な範囲です(-s).\n");
	break;
    case 'u':
	Disasm_UnusedTrapUndefined = FALSE;
	if (optarg && (ck_atoi (optarg) == 1))
	    Disasm_SX_Window = TRUE;
	break;
    case 'v':
	option_v = TRUE;
	break;
    case 'w':
	Data_width = ck_atoi (optarg);
	if( Data_width < 1 || 32 < Data_width )
	    err ("値が無効な範囲です(-w).\n");
	break;
    case 'x':
	option_x = TRUE;
	break;
    case 'y':
	option_y = TRUE;
	break;
    case 'z':
	option_z = TRUE;
	Absolute = ABSOLUTE_ZOPT;
	FileType = 'r';
	{
	    char* p;
#ifndef	__LIBC__
	    char* new_optarg = (char*) Malloc (strlen (optarg) + 1);
	    optarg = strcpy (new_optarg, optarg);
#endif
	    p = strchr (optarg, ',');
	    if (p) {
		*p++ = '\0';
		Base = (address) atox (optarg);
		Exec = (address) atox (p);
	    } else
		Exec = Base = (address) atox (optarg);
#ifndef	__LIBC__
	    Mfree (new_optarg);
#endif
	}
	if (Base > Exec) {
	    err ("値が無効な範囲です(-z).\n");
	}
	break;

    case 'A':
	Disasm_MnemonicAbbreviation = TRUE;
	break;
    case 'B':
	option_B = TRUE;
	break;
    case 'C':
	SymbolColonNum = optarg ? ck_atoi (optarg) : 0;
	if( SymbolColonNum < 0 || 3 < SymbolColonNum )
	    err ("値が無効な範囲です(-C).\n");
	break;
    case 'D':
	option_D = TRUE;
	break;
    case 'E':
	option_E = TRUE;
	break;
    case 'F':
	Disasm_Dbra = FALSE;
	break;
    case 'G':
	Arg_after_call = TRUE;
	break;
    case 'I':
	option_I = TRUE;
	break;
    case 'K':
	if (!optarg[0] || optarg[1])
	    err ("コメントキャラクタは一文字しか指定出来ません(-K).\n");
	CommentChar = *optarg;
	break;
    case 'L':
	if (!optarg[0] || optarg[1])
	    err ("ラベルの先頭文字は一文字しか指定出来ません(-L).\n");
	if ((char)'0' <= *optarg && *optarg <= (char)'9')
	    err ("ラベルの先頭文字に数字は使えません(-L).\n");
	Label_first_char = *optarg;
	break;
    case 'M':
	option_M = TRUE;
	break;
    case 'N':
	option_N = TRUE;
	break;
    case 'P':
	Emulate_mode = ck_atoi (optarg);
	if (Emulate_mode & ~0x03)
	    err ("値が無効な範囲です(-P).\n");
	break;
    case 'Q':
    /*  option_Q = TRUE;  */
	break;
    case 'R':
	UndefRegLevel = ck_atoi (optarg);
	if (UndefRegLevel & ~0x0f)
	    err ("値が無効な範囲です(-R).\n");
	break;
    case 'S':
	option_S = TRUE;
	Output_SplitByte = ( optarg ? ck_atoi (optarg) : 64 ) * 1024;
	break;
    case 'T':
	option_T = TRUE;
	if (optarg)
	    Tablefilename = optarg;
	break;
    case 'U':
	option_U = TRUE;
	strupr (opsize);
	/* fall through */
    case 'X':
	strupr (Hex);
	break;
    case 'V':
	Verbose = ck_atoi (optarg);
	if( Verbose < 0 || 2 < Verbose )
	    err ("値が無効な範囲です(-V).\n");
	break;
    case 'W':
	Compress_len = ck_atoi (optarg);
	break;
    case 'Y':
	option_Y = TRUE;
	break;
    case 'Z':
	option_Z = TRUE;
	Zerosupress_mode = optarg ? ck_atoi(optarg) : 0;
	if( Zerosupress_mode < 0 || 1 < Zerosupress_mode )
	    err ("値が無効な範囲です(-Z).\n");
	break;
    case '#':
	Debug = ck_atoi (optarg);
	break;
    case '-':
	{
#ifndef	OSKDIS
	    const char** p;
	    char c = optarg[0];
	    if (!optarg[1] && (c == (char)'x' || c == (char)'r' || c == (char)'z')) {
		FileType = c;
		break;
	    }
#endif

#define isLONGOPT(str) (strcmp (optarg, str) == 0)
	         if (isLONGOPT (   "fpsp"))	Emulate_mode |=  1;
	    else if (isLONGOPT ("no-fpsp"))	Emulate_mode &= ~1;
	    else if (isLONGOPT (   "isp"))	Emulate_mode |=  2;
	    else if (isLONGOPT ("no-isp"))	Emulate_mode &= ~2;
	    else if (isLONGOPT ("no-fpu"))	{ FPCP_type = 0; init_fpuid_table (); }
	    else if (isLONGOPT ("no-mmu"))	MMU_type = 0;

	    else if (isLONGOPT ("sp"))		sp_a7_flag = TRUE;
	    else if (isLONGOPT ("a7"))		sp_a7_flag = FALSE;
	    else if (isLONGOPT ("old-syntax"))	Old_syntax = TRUE;
	    else if (isLONGOPT ("new-syntax"))	Old_syntax = FALSE;
	    else if (isLONGOPT ("inreal"))	Inreal_flag = TRUE;
	    else if (isLONGOPT ("real"))	Inreal_flag = FALSE;

	    else if (isLONGOPT ("overwrite"))	option_overwrite = TRUE;
	    else if (isLONGOPT ("help"))	usage (0);
	    else if (isLONGOPT ("version"))	{ printf ("dis version %s\n", Version);
						  exit (0); }
#ifndef	OSKDIS
	    else if (strncmp (optarg, "header=", 7) == 0)
		Header_filename = optarg + 7;
	    else if ((p = include_option (optarg)) != NULL)
		*p = include_filename (optarg);
#endif
	    else
		err ("%s: unrecognized option '--%s'\n", argv[0], optarg);
	}
	break;
/*  case '?':  */
    default:
	exit (EXIT_FAILURE);
	break;
    }
    return;
}


extern void
usage (int exitcode)
{
    static const char message[] =
	"usage: dis [option] 実行ファイル名 [出力ファイル名]\n"
	"option:\n"

	/* 小文字オプション */
	"	-a[num]		num 行ごとにアドレスをコメントで入れる(num を省略すると 5 行ごと)\n"
	"	-b num		相対分岐命令のサイズの出力(0:自動 1:常に省略 2:常に付ける)\n"
	"	-c		ラベルチェックを行わない\n"
	"	-d		デバイスドライバの時に指定\n"
	"	-e[file]	ラベルファイルの出力\n"
	"	-f		バイト操作命令の不定バイトのチェック($00 or $ff ?)をしない\n"
	"	-g[file]	ラベルファイルを読み込む\n"
	"	-h		データ領域中の $4e75(rts)の次のアドレスに注目する\n"
	"	-i		分岐先で未定義命令があってもデータ領域と見なさない\n"
	"	-j		アドレスエラーが起こるであろう命令を未定義命令と見なさない\n"
	"	-k		命令の中を指すラベルはないものと見なす\n"
	"	-l		プログラム領域が見つからなくなるまで何度も捜すことをしない\n"
	"	-m 680x0[,...]	逆アセンブル対象の MPU を指定(68000-68060,680x0)\n"
	"	-m 68851	68851 命令を有効にする(-m68020 指定時のみ有効)\n"
	"	-m 6888x[,ID]	有効な FPCP とその ID を指定(68881/68882 ID=0-7,省略時1)\n"
	"	-n num		文字列として判定する最小の長さ. 0 なら判定しない(初期値=3)\n"
	"	-o num		文字列領域の桁数(1≦num≦80 初期値=60)\n"
	"	-p		データ領域中のプログラム領域を判別しない\n"
	"	-q[num]		メッセージを出力しない([0]:通常 1:テーブルに関する情報も出力しない)\n"
	"	-r		文字列に 16 進数のコメントを付ける\n"
	"	-s[num]		シンボルテーブルの出力([0]:しない 1:[通常] 2:全て)\n"
/* -t */
	"	-u[num]		未使用の A,F line trap を未定義命令と見なさない(num=1 SX-Window対応)\n"
	"	-v		単なる逆アセンブルリストの出力\n"
	"	-w num		データ領域の横バイト数(1≦num≦32 初期値=8)\n"
	"	-x		実際のオペコードを 16 進数のコメントで付ける\n"
	"	-y		全てのデータ領域をプログラム領域でないか確かめることをしない\n"
	"	-z base,exec	実行ファイルを base からのバイナリファイルとみなし、exec から解析する\n"

	/* 大文字オプション */
	"\n"
	"	-A		cmpi, movea 等を cmp, move 等にする\n"
	"	-B		bra の後でも改行する\n"
	"	-C[num]		ラベルの後のコロン(0:付けない 1:全てに1つ [2]:通常 3:全てに2つ)\n"
	"	-D		データセクション中にもプログラムを認める\n"
	"	-E		バイト操作命令の不定バイトの書き換えチェックをしない\n"
	"	-F		dbra,fdbra を dbf,fdbf として出力する\n"
	"	-G		サブルーチンコールの直後に引数を置くプログラムを解析する\n"
/* -H */
	"	-I		命令の中を差すラベルのアドレスを表示する\n"
/* -J */
	"	-K char		char をコメントキャラクタとして用いる\n"
	"	-L char		char をラベル名の先頭文字として用いる\n"
	"	-M		cmpi, move, addi.b, subi.b #imm および pack, unpk にコメントをつける\n"
	"	-N		サイズがデフォルトなら付けない\n"
/* -O */
	"	-P num		ソフトウェアエミュレーション命令を有効にする(ビット指定, 初期値=3)\n"
	"		+1	未実装浮動小数点命令を有効にする\n"
	"		+2	未実装整数命令を有効にする\n"
/*	"	-Q		環境変数 dis_opt を参照しない\n"	*/
	"	-R num		未使用フィールドのチェック項目の指定(ビット指定, 初期値=15)\n"
	"		+1	mulu.l, muls.l, ftst.x における未使用レジスタフィールドのチェック\n"
	"		+2	拡張アドレッシングでのサプレスされたレジスタフィールドのチェック\n"
	"		+4	サプレスされたインデックスレジスタに対するスケーリングのチェック\n"
	"		+8	サプレスされたインデックスレジスタに対するサイズ指定(.l)のチェック\n"
	"	-S[num]		出力ファイルを num KB ごとに分割する(num を省略すると 64KB)\n"
	"	-T[file]	テーブル記述ファイルを読み込む\n"
	"	-U		ニーモニックを大文字で出力する\n"
	"	-V num		バックトラックの原因の表示(0:しない [1]:プログラム領域 2:全ての領域)\n"
	"	-W num		同一データを .dcb で出力する最小バイト数. 0なら圧縮しない(初期値=64)\n"
	"	-X		16 進数を大文字で出力する\n"
	"	-Y		カレントディレクトリからも include ファイルを検索する\n"
	"	-Z[num]		16 進数をゼロサプレスする([0]:通常 1:省略可能な'$'を省略)\n"

	/* 単語名オプション */
	"\n"

#ifndef	OSKDIS
	"	--include-XXX-mac=file	include ファイルの指定(XXX = doscall,iocscall,fefunc)\n"
	"	--exclude-XXX-mac	include ファイルを読み込まない\n"
#endif
	"	--header=file	ヘッダファイルの指定(環境変数 dis_header より優先)\n"
	"	--(no-)fpsp	未実装浮動小数点命令を[有効](無効)にする\n"
	"	--(no-)isp	未実装整数命令を[有効](無効)にする\n" 
	"	--no-fpu	内蔵 FPU 命令を無効にする(-m68040～68060 の後に指定)\n" 
	"	--no-mmu	内蔵 MMU 命令を無効にする(-m68030～68060 の後に指定)\n" 
	"	--sp		a7 レジスタを'sp'と表記する(標準では --a7)\n"
	"	--old-syntax	アドレッシングを旧表記で出力する(標準では --new-syntax)\n"
	"	--(in)real	浮動小数点を[実数表記](内部表現)で出力する\n"
	"	--overwrite	ファイルを無条件で上書きする\n"
	"	--version	バージョンを表示する\n"
	"	--help		使用法を表示する\n"

#if 0	/* 隠しオプション */
	"\n"
#ifndef	OSKDIS
	"	--x|r|z		実行ファイルを X|R|Z 形式と見なす\n"
#endif
	"	-# num		デバッグモード\n"
#endif
	; /* end of message */

    print_title ();
    printf (message);
    exit (exitcode);
}

/* EOF */
