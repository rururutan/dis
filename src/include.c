/* $Id: include.c,v 1.1 1996/11/07 08:03:42 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	インクルードファイル読み込みモジュール
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <stdlib.h>	/* getenv() */
#include <string.h>

#include "disasm.h"	/* OSlabel , FElabel , SXlabel */
#include "estruct.h"
#include "etc.h"
#include "generate.h"	/* IOCSlabel */
#include "global.h"
#include "include.h"


USEOPTION	option_Y;


/* External variables */
#ifdef	OSKDIS
const char*  OS9_mac_path;
const char* MATH_mac_path;
const char*  CIO_mac_path;
#else
const char*  Doscall_mac_path;
const char* Iocscall_mac_path;
const char*   Fefunc_mac_path;
const char*   Sxcall_mac_path;

const char*  doscall_mac = "doscall.mac";
const char* iocscall_mac = "iocscall.mac";
const char*   fefunc_mac = "fefunc.mac";
#endif	/* OSKDIS */


/* static 関数プロトタイプ */
#ifndef	OSKDIS
private void	search_and_readfile (const char** bufptr, const char* filename,
			char*** label, int hbyte, char* callname, int max);
#endif	/* !OSKDIS */
private int	readfile (const char* filename, char*** label,
			int hbyte, char* callname, int max);


#ifdef	OSKDIS
/*

  os9defs.d mathdefs.d ciodefs.d を
  環境変数 DISDEFS にしたがって読み込む.

*/

extern void
load_OS_sym (void)
{
    static const char os9defs_d[] = "os9defs.d";
    static const char mathdefs_d[] = "mathdefs.d";
    static const char ciodefs_d[] = "ciodefs.d";
    char* dis_inc;
    int plen;

    eprintf ("%s %s %s を読み込みます.\n", os9defs_d, mathdefs_d, ciodefs_d);

    if ((dis_inc = getenv ("DISDEFS")) == NULL) {
    /*  err ("環境変数 DISDEFS が設定されていません.\n");  */
	eprintf ("環境変数 DISDEFS が設定されていません.\n");
	return;
    }
    plen = strlen (dis_inc);

    OS9_mac_path = Malloc (plen + sizeof os9defs_d + 2);
    strcat (strcat (strcpy (OS9_mac_path, dis_inc), "/"), os9defs_d);
    if (!readfile (OS9_mac_path, OS9label, 0, "OS9")) {
	eprintf ("%s をオープン出来ません.\n", os9defs_d);
	OS9_mac_path = NULL;
    }

    MATH_mac_path = Malloc (plen + sizeof mathdefs_d + 2);
    strcat (strcat (strcpy (MATH_mac_path, dis_inc), "/"), mathdefs_d);
    if (!readfile (MATH_mac_path, MATHlabel, 0, "T$Math")) {
	eprintf ("%s をオープン出来ません.\n", mathdefs_d);
	MATH_mac_path = NULL;
    }

    CIO_mac_path = Malloc (plen + sizeof ciodefs_d + 2);
    strcat (strcat (strcpy (CIO_mac_path, dis_inc), "/"), ciodefs_d);
    if (!readfile (CIO_mac_path, CIOlabel, 0, "CIO$Trap")) {
	eprintf ("%s をオープン出来ません.\n", ciodefs_d);
	CIO_mac_path = NULL;
    }
}

#else	/* !OSKDIS */
/*

  doscall.mac iocscall.mac fefunc.mac をカレントディレクトリ若しくは
  環境変数 (dis_)include にしたがって読み込む.

*/

extern void
load_OS_sym (void)
{

    search_and_readfile (&Doscall_mac_path, doscall_mac, &OSlabel,  0xff,  OSCallName, 256);
    search_and_readfile (&Iocscall_mac_path,iocscall_mac,&IOCSlabel,0x00,IOCSCallName, 256);
    search_and_readfile (&Fefunc_mac_path,  fefunc_mac,  &FElabel,  0xfe,  FECallName, 256);

    if (Disasm_SX_Window
	&& (Sxcall_mac_path = getenv ("dis_sxmac")) != NULL
	&& *Sxcall_mac_path) {
	if (!readfile (Sxcall_mac_path, &SXlabel, 0x0a, SXCallName, SXCALL_MAX))
	    err ("%s をオープン出来ません.\n" , Sxcall_mac_path);
    }
}
#endif	/* OSKDIS */


#ifndef	OSKDIS
/*

  1) ./ (-Y オプション指定時のみ)
  2) $dis_include/
  3) $include/
  の順に readfile() を試す.

*/
private void
search_and_readfile (const char** bufptr, const char* filename,
			char*** label, int hbyte, char* callname, int max)
{
    char* dis_inc = NULL;
    char* include = NULL;
    int path_flag;

    /* --exclude-***call 指定時は読み込まない */
    if (filename == NULL)
	return;

    path_flag = strchr (filename, ':') || strchr (filename, '/');

    /* -Y オプション指定時はカレントディレクトリから検索する. */
    /* ファイル名にパスデリミタが含まれる場合もそのまま検索. */
    if (path_flag || option_Y) {
	if (readfile (filename, label, hbyte, callname, max)) {
	    *bufptr = filename;
	    return;
	}
    }

    /* パスデリミタが含まれなければ環境変数を参照する. */
    if (!path_flag) { 
	int flen = strlen (filename) + 2;
	char* buf;

	/* 環境変数 dis_include のパスから検索する. */
	if ((dis_inc = getenv ("dis_include")) != NULL) {
	    buf = (char*) Malloc (strlen (dis_inc) + flen);
	    strcat (strcat (strcpy (buf, dis_inc), "/"), filename);
	    if (readfile (buf, label, hbyte, callname, max)) {
		*bufptr = buf;
		return;
	    }
	    Mfree (buf);
	}

	/* 環境変数 include のパスから検索する. */
	if ((include = getenv ("include")) != NULL) {
	    buf = (char*) Malloc (strlen (include) + flen);
	    strcat (strcat (strcpy (buf, include), "/"), filename);
	    if (readfile (buf, label, hbyte, callname, max)) {
		*bufptr = buf;
		return;
	    }
	    Mfree (buf);
	}
    }

    if (path_flag || dis_inc || include)
	err ("%s をオープン出来ません.\n", filename);
    else
	err ("環境変数 (dis_)include が設定されていません.\n");
}
#endif	/* !OSKDIS */


/*
	includeファイルから読み込んだ文字列を
	シンボル名、疑似命令名、数字(または仮引数名)
	に分割し、各文字列へのポインタを設定して返す.

	文字列の内容が正しくなかった場合は0、正しければ1を返す.
*/

#define IS_EOL(p) ((*(p) < 0x20) || \
	(*(p) == (char)'*') || (*(p) == (char)';') || (*(p) == (char)'#'))

private int
separate_line (char* ptr, char** symptr, char** psdptr, char** parptr)
{
    char* p = ptr;
    unsigned char c;

    /* 行頭の空白を飛ばす */
    while ((*p == (char)' ') || (*p == (char)'\t'))
	p++;

    /* 空行または注釈行ならエラー */
    if (IS_EOL (p))
	return 0;

    /* シンボル名の収得 */
    *symptr = p;

    /* シンボル名の末尾をNULにする */
    p = strpbrk (p, ":. \t");
    if (p == NULL)
	return 0;
    c = *p;
    *p++ = '\0';

    /* シンボル定義の':'、空白、疑似命令の'.'を飛ばす */
    while (c == (char)':')
	c = *p++;
    while ((c == (char)' ') || (c == (char)'\t'))
	c = *p++;
    if (c == (char)'.')
	c = *p++;
    p--;

    /* 疑似命令が記述されていなければエラー */
    if (IS_EOL (p))
	return 0;

    /* 疑似命令へのポインタを設定 */
    *psdptr = p;

    /* 疑似命令名の末尾をNULにする */
    p = strpbrk (p, " \t");
    if (p == NULL)
	return 0;
    *p++ = '\0';

    /* パラメータまでの空白を飛ばす */
    while ((*p == (char)' ') || (*p == (char)'\t'))
	p++;

    /* パラメータが記述されていなければエラー */
    if (IS_EOL (p))
	return 0;

    /* パラメータへのポインタを設定 */
    *parptr = p;

    /* パラメータの末尾をNULにする */
    while (*(unsigned char*)p++ > 0x20)
	;
    *--p = '\0';

    return 1;
}


/*

	アセンブラのequ/macro疑似命令の行を読み取る.
	シンボル定義を読み取ったら1を返し、それ以外は0を返す.
	(二度目の定義を無視した時や、macro定義なども0)

*/

private int
getlabel (char* linebuf, char** label, int hbyte, char* callname)
{
    char* symptr;
    char* psdptr;
    char* parptr;

    if (separate_line (linebuf, &symptr, &psdptr, &parptr)) {

	/* コール名 equ コール番号 */
	if (strcasecmp (psdptr, "equ") == 0) {
	    int val;
	    if (*parptr == (char)'$')
		parptr++;
	    else if (strncasecmp (parptr, "0x", 2) == 0)
		parptr += 2;
	    else
		return 0;
	    val = atox (parptr);

	    if (hbyte == 0xa) {		/* SXCALL */
		if (((val >> 12) == hbyte) && !label[val & 0xfff]) {
		    label[val & 0xfff] = strcpy (Malloc (strlen (symptr) + 1), symptr);
		    return 1;
		}
	    }
	    else {			/* DOS, IOCS, FPACK */
		if (((val >> 8) == hbyte) && !label[val & 0xff]) {
		    label[val & 0xff] = strcpy (Malloc (strlen (symptr) + 1), symptr);
		    return 1;
		}
	    }
	}

	/* マクロ名: .macro 実引数名 */
	else if (strcasecmp (psdptr, "macro") == 0) {
	    if (strlen (symptr) < MAX_MACRO_LEN)
		strcpy (callname, symptr);
#if 0
	    else
		eprintf ("Warning : マクロ名が長すぎます(%s)\n", symptr);
#endif
	}
    }
    return 0;
}


/*
	ファイルからシンボル定義を読み込む.
	ファイルのオープンに失敗した場合は0を返し、それ以外は1を返す.
	シンボルが一個も定義されていなくても警告を表示するだけで
	アボートはしない.
*/
private int
readfile (const char* filename, char*** label, int hbyte, char* callname, int max)
{
    int label_num;
    FILE* fp;

    if ((fp = fopen (filename, "rt")) == NULL)
	return 0;

    /* ファンクション名への配列を確保 */
    {
	char** p;
	int i;

	p = *label = Malloc (sizeof (char*) * max);
	for (i = 0; i < max; i++)
	    *p++ = NULL;
    }

    eprintf ("Reading %s...\n", filename);
    {
	char linebuf[256];

	label_num = 0;
	while (fgets (linebuf, sizeof linebuf, fp))
	    label_num += getlabel (linebuf, *label, hbyte, callname);
    }
    fclose (fp);

    if (label_num == 0)
#if 0
	err ("Error : %s にはシンボルが１つも定義されていません.\n", filename);
#else
	eprintf ("Warning : %s にはシンボルが１つも定義されていません.\n", filename);
#endif
    return 1;
}


/* EOF */
