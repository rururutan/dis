/* $Id: estruct.h,v 1.1 1996/10/24 04:27:44 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	構造体定義ファイル
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	ESTRUCT_H
#define	ESTRUCT_H

#ifdef __HUMAN68K__
#define __attribute__(x) /* NOTHING */
#endif

/* #include <class.h> */
typedef unsigned char	UBYTE;
typedef short		WORD;
typedef unsigned short	UWORD;
typedef long		LONG;
typedef unsigned long	ULONG;


#ifdef	TRUE
#undef	TRUE
#endif
#ifdef	FALSE
#undef	FALSE
#endif
typedef enum { FALSE, TRUE } boolean;

typedef enum {
    BYTESIZE,		/* バイト */
    WORDSIZE,		/* ワード */
    LONGSIZE,		/* ロングワード */
    QUADSIZE,		/* クワッドワード */
    SHORTSIZE,		/* ショート( 相対分岐命令 ) */

    SINGLESIZE,		/* 32bit実数型 */
    DOUBLESIZE,		/* 64bit実数型 */
    EXTENDSIZE,		/* 96bit実数型 */
    PACKEDSIZE,		/* 96bitBCD実数型 */
    NOTHING,		/* 無し */

    STRING,		/* 文字列 */
    RELTABLE,		/* リロケータブルオフセットテーブル */
    RELLONGTABLE,	/* ロングワードなリロケータブルオフセットテーブル */
    ZTABLE,		/* 絶対番地テーブル */
#ifdef	OSKDIS
    WTABLE,		/* ワードテーブル */
#endif	/* OSKDIS */

    EVENID,
    CRID,
    WORDID,
    LONGID,
    BYTEID,
    ASCIIID,
    ASCIIZID,
    LASCIIID,
    BREAKID,
    ENDTABLEID,
    UNKNOWN = 128	/* 不明 */
} opesize;


typedef UBYTE*	address;


typedef struct {	/* .x ファイルのヘッダ */
    UWORD   head;
    char    reserve2;
    char    mode;
    address base;
    address exec;
    ULONG   text;
    ULONG   data;
    ULONG   bss;
    ULONG   offset;
    ULONG   symbol;
    char    reserve[ 0x1c ];
    ULONG   bindinfo;
} __attribute__ ((packed)) xheader;

typedef struct {	/* .z ファイルのヘッダ */
    UWORD   header;
    ULONG   text;
    ULONG   data;
    ULONG   bss;
    char    reserve[8];
    address base;
    UWORD   pudding;
} __attribute__ ((packed)) zheader;


#ifdef	OSKDIS
typedef struct {	/* OS-9/680x0 モジュールのヘッダ    */
    UWORD   head;	/* +00 $4AFC			    */
    UWORD   sysrev;	/* +02 リビジョンＩＤ		    */
    ULONG   size;	/* +04 モジュールサイズ		    */
    ULONG   owner;	/* +08 オーナＩＤ		    */
    address name;	/* +0C モジュール名オフセット	    */
    UWORD   accs;	/* +10 アクセス許可		    */
    UWORD   type;	/* +12 モジュールタイプ／言語	    */
    UWORD   attr;	/* +14 属性／リビジョン		    */
    UWORD   edition;	/* +16 エディション		    */
    address usage;	/* +18 使い方コメントのオフセット   */
    address symbol;	/* +1C シンボルテーブル		    */
    char    resv[14];	/* +20 予約済み			    */
    UWORD   parity;	/* +2E ヘッダパリティ		    */
    address exec;	/* +30 実行オフセット		    */
    address excpt;	/* +34 ユーザトラップエントリ	    */
    ULONG   mem;	/* +38 メモリサイズ		    */
    ULONG   stack;	/* +3C スタックサイズ		    */
    address idata;	/* +40 初期化データオフセット	    */
    address irefs;	/* +44 初期化参照オフセット	    */
    address init;	/* +48 初期化実行エントリ(TRAPLIB)  */
    address term;	/* +4C 終了実行エントリ(TRAPLIB)    */
} __attribute__ ((packed)) os9header;
#endif	/* OSKDIS */


#endif	/* ESTRUCT_H */

/* EOF */
