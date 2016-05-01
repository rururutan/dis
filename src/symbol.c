/* $Id: symbol.c,v 1.1 1996/11/07 08:04:02 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	シンボルネーム管理モジュール
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <string.h>

#include "estruct.h"
#include "etc.h"
#include "global.h"
#include "hex.h"		/* itoxd() */
#include "label.h"
#include "output.h"		/* outputf() */
#include "symbol.h"


USEOPTION	option_g;
short	Output_Symbol = 1;	/* 0:しない 1:定数のみ 2:.xdef も */


#define	SYM_BUF_UNIT	16		/* バッファ拡大時の増加要素数 */
static int	Symbolbufsize;		/* シンボルバッファの数 */
static symbol*	Symtable;		/* シンボルテーブルの頭 */
static int	Symnum;			/* シンボルテーブルの要素数 */


extern void
init_symtable (void)
{
    return;
}

/*

  後始末

  シンボル名リストとシンボル名バッファも解放する必要があるけど手抜き.

*/
extern void
free_symbuf (void)
{
    Mfree (Symtable);
}

extern boolean
is_exist_symbol (void)
{
    return Symnum ? TRUE : FALSE;
}


#ifndef	OSKDIS

/*
	ソースコードにシンボル定義を出力する

	引数:
		op_equ	.equ 疑似命令("\t.equ\t")
		colon	コロン("::" -C オプションによって変更)
	返値
		出力したシンボル数(行数)

	Human68k の .x 型実行ファイル形式に依存.
*/
extern int
output_symbol_table (const char* op_equ, const char* op_xdef, const char* colon)
{
    char* ptr = (char*)(Top + Head.text + Head.data + Head.offset);
    ULONG end = (ULONG)ptr + Head.symbol
	      - (sizeof (UWORD) + sizeof (address) + 2);
    int out_xdef = (Output_Symbol == 2);
    int count = 0;

    if ((int)ptr & 1 || !Output_Symbol)
	return count;

    while ((ULONG)ptr <= end) {
	UWORD type;
	address adrs;

	type = peekw (ptr);
	ptr += sizeof (UWORD);
	adrs = (address) peekl (ptr);
	ptr += sizeof (address);

	if (*ptr && *ptr != (char)'*') {
	    char buf[16];
	    char* tab;

	    switch (type) {
	    case XDEF_ABS:
		/* 「xxx:: .equ $nn」の形式で出力する */
				  /* TAB-2 */
		tab = (strlen (ptr) < (8-2)) ? "\t" : "";
		itoxd (buf, (ULONG) adrs, 8);
		outputf ("%s%s%s%s%s" CR, ptr, colon, tab, op_equ, buf);
		count++;
		break;

	    case XDEF_COMMON:
	    case XDEF_TEXT:
	    case XDEF_DATA:
	    case XDEF_BSS:
	    case XDEF_STACK:
		if (out_xdef) {
		    outputf ("%s%s" CR, op_xdef, ptr);
		    count++;
		}
		break;

	    default:
		break;
	    }
	}

	/* 今処理したシンボルを飛ばす */
	while (*ptr++)
	    ;
	ptr += (int)ptr & 1;
    }

    return count;
}


/*
	ラベルファイルで定義されたシンボルの属性を
	シンボル情報の属性に変更する

	Human68k の .x 型実行ファイル形式に依存.
*/
static void
change_sym_type (symbol* symbolptr, int type, char* ptr)
{
    symlist* sym = &symbolptr->first;

    do {
	if (strcmp (sym->sym, ptr) == 0) {
	    sym->type = type;
	    return;
	}
	sym = sym->next;
    } while (sym);
}

/*
	実行ファイルに付属するシンボルテーブルを登録する
	ラベルファイルの読み込みより後に呼び出される

	Human68k の .x 型実行ファイル形式に依存.
*/
extern void
make_symtable (void)
{
    char* ptr = (char*)(Top + Head.text + Head.data + Head.offset);
    ULONG end = (ULONG)ptr + Head.symbol
	      - (sizeof (UWORD) + sizeof (address) + 2);

    if ((int)ptr & 1)
	return;

    while ((ULONG)ptr <= end) {
	UWORD type;
	address adrs;

	type = peekw (ptr);
	ptr += sizeof (UWORD);
	adrs = (address) peekl (ptr);
	ptr += sizeof (address);

	if (!*ptr) {
#if 0
	    eprintf ("シンボル名が空文字列です(%#.6x %#.6x)", type, adrs);
#endif
	} else if (*ptr == (char)'*') {
	    eprintf ("アドレス境界情報のシンボルです(%#.6x %#.6x %s)\n",
							type, adrs, ptr);
	}

	else {
	    symbol* sym;

	    /* スタックサイズの収得 */
	    if (type == XDEF_STACK && BeginBSS <= adrs && adrs < BeginSTACK)
		BeginSTACK = adrs;

	    switch (type) {
	    case XDEF_ABS:
		break;

	    case XDEF_COMMON:
		type = XDEF_BSS;
		/* fall through */
	    case XDEF_STACK:
	    case XDEF_TEXT:
	    case XDEF_DATA:
	    case XDEF_BSS:
		sym = symbol_search (adrs);

		if (!sym)
		    regist_label (adrs, DATLABEL | UNKNOWN | SYMLABEL);

		/* -g 指定時はシンボルテーブルから登録しない */
		/* ただし、属性だけは利用する		     */
		if (!option_g)
		    add_symbol (adrs, type, ptr);
		else if (sym)
		    change_sym_type (sym, type, ptr);
		break;

	    default:
		eprintf ("未対応のシンボル情報です(%#.6x %#.6x %s)\n",
							type, adrs, ptr);
		break;

	    }
	}

	/* 今処理したシンボルを飛ばす */
	while (*ptr++)
	    ;
	ptr += (int)ptr & 1;
    }
}


/*
	指定した属性のシンボルを探す
*/
extern symlist*
symbol_search2 (address adrs, int type)
{
    symbol* symbolptr = symbol_search (adrs);

    if (symbolptr) {
	symlist* sym = &symbolptr->first;

	while (sym->type != (UWORD)type && (sym = sym->next))
	    ;
	return sym;
    }
    return (symlist*)NULL;
}

#endif	/* !OSKDIS */


extern void
add_symbol (address adrs, int type, char *symstr)
{			   /* type == 0 : labelfileでの定義 */
    int  i;
    symbol* sym = symbol_search (adrs);

    /* 既に登録済みならシンボル名を追加する */
    if (sym) {
	symlist* ptr = &sym->first;

	while (ptr->next)
	    ptr = ptr->next;

	/* symlist を確保して、末尾に繋げる */
	ptr = ptr->next = Malloc (sizeof (symlist));

	/* 確保した symlist を初期化 */
	ptr->next = NULL;
	ptr->type = (UWORD)type;
	ptr->sym = symstr;

	return;
    }

    if (Symnum == Symbolbufsize) {
	/* バッファを拡大する */
	Symbolbufsize += SYM_BUF_UNIT;
	Symtable = Realloc (Symtable, Symbolbufsize * sizeof (symbol));
    }

    for (i = Symnum - 1; (i >= 0) && (Symtable[ i ].adrs > adrs); i--)
	Symtable[ i + 1 ] = Symtable[ i ];

    Symnum++;
    sym = &Symtable[ ++i ];
    sym->adrs = adrs;

    /* 最初のシンボルを記録 */
    sym->first.next = NULL;
    sym->first.type = (UWORD)type;
    sym->first.sym  = symstr;

#ifdef	DEBUG
    printf ("type %.4x adrs %.6x sym:%s\n", sym->first.type, sym->.adrs, sym->first.sym);
#endif
}


/*

  adrs をシンボルテーブルから捜す
  adrs は common text data bss のいずれか
  見つかったらポインタ、でなければ NULL を返す

*/
extern symbol*
symbol_search (address adrs)
{
    int step;
    symbol* ptr;

    /* 多少は速くなる? */
    if (Symnum == 0)
	return NULL;

    ptr = Symtable;
    for (step = Symnum >> 1; step > 4; step >>= 1)
	if ((ptr + step)->adrs <= adrs)		/* binary search */
	    ptr += step;

    for (; ptr < Symtable + Symnum; ptr++) {
	if (ptr->adrs == adrs)
	    return ptr;
	else
	    if (adrs < ptr->adrs)
		return NULL;
    }
    return NULL;
}


/* 以下は未使用関数 */

#if 0
extern void
del_symbol (address adrs)
{
    symbol* sym = symbol_search (adrs);

    if (sym) {
	symlist* ptr = sym->first.next;

	while (ptr) {
	    symlist* next = ptr->next;
	    Mfree (ptr);
	    ptr = next;
	}

	Symnum--;
	for (; sym < &Symtable[ Symnum ]; sym++)
	    *sym = *(sym + 1);
    }
}
#endif

/* EOF */
