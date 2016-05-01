/*
 * PROJECT C Library, X68000 PROGRAMMING INTERFACE DEFINITION
 * --------------------------------------------------------------------
 * This file is written by the Project C Library Group,	 and completely
 * in public domain. You can freely use, copy, modify, and redistribute
 * the whole contents, without this notice.
 * --------------------------------------------------------------------
 * $Id: getopt.c,v 1.1 1996/11/07 08:03:38 ryo freeze $
 */

/* System headers */
#include <stdio.h>
#include <stdlib.h>

#include "getopt.h"

char *optarg;					/* 引数を指すポインタ */
int optind = -1;				/* ARGV の現在のインデックス */
static int opterr = 1;				/* エラー表示フラグ */
static int _getopt_no_ordering;			/* ORDER フラグ */

/* File scope functions */
static void rotate_right (char *vector[], int size)
{
    char *temp;

    /* 最後尾を保存 */
    temp = vector[size - 1];

    /* size 分ローテート */
    while (size--)
	vector[size] = vector[size - 1];

    /* 先頭に保存していたものを... */
    vector[0] = temp;
}

/* Functions */
int dis_getopt (int argc, char *argv[], const char *options)
{
    int index;					/* 現在のインデックス */
    int next;					/* 次のインデックス */
    int optchar;				/* オプション文字 */
    char *string;				/* オプション文字列 */
    const char *ptr;				/* options 探査用 */

    static int init;				/* 次回初期化する必要あり */
    static int savepoint;			/* 入れ換え用の記憶ポイント */
    static int rotated;				/* 交換フラグ */
    static int comidx;				/* 複合インデックス */

#ifdef __LIBC__
#define ERR(fmt,arg1,arg2)	if (opterr) eprintf(fmt,arg1,arg2)
#else
#define ERR(fmt,arg1,arg2)	if (opterr) fprintf(stderr,fmt,arg1,arg2)
#endif

    /* 初期化の必要があれば初期化する */
    if (init || optind < 0) {
	optind = 1;
	optarg = 0;
	rotated = 0;
	init = 0;
	comidx = 1;
	savepoint = 0;
    }

    /* 捜査開始位置を設定 */
    index = optind;

  again:

    /* 引数を取り出す */
    string = argv[index];
    next = index + 1;

    /* すでに終りか？ */
    if (string == 0) {
	if (savepoint > 0)
	    optind = savepoint;
	init = 1;
	return EOF;
    }

    /* '-' で始まるか？ */
    if (*string == '-') {

	/* フラグ分ポインタを進める */
	string += comidx;

	/* 正確に "-" か？ならば普通の引数 */
	if ((optchar = *string++) == '\0')
	    goto normal_arg;

	/* ORDERING の必要があれば引数列を部分的にローテート */
	if (savepoint > 0) {
	    rotate_right (&argv[savepoint], index - savepoint + 1);
	    rotated = 1;
	    index = savepoint;
	    savepoint++;
	}

	/* 正確に "--" ならば強制的に捜査を終りにする */
	if (optchar == '-' && *string == '\0' && comidx == 1){
	    init = 1;
	    optchar = EOF;
	    goto goback;
	}

	/* オプション文字群の中から該当するものがあるか調べる */
	for (ptr = options; *ptr; ptr++)
	    if (*ptr != ':' && *ptr == optchar)
		break;

	/* 引数を伴う場合ならばインデックスは初期化 */
	if (*string == '\0' || ptr[1] == ':')
	    comidx = 1;

	/* さもなければ複合オプションインデックスを加算 */
	else {
	    comidx++;
	    index--;
	}

	/* 結局見つからなかったなら... */
	if (*ptr == '\0') {
	    ERR ("%s: unrecognized option '-%c'\n", argv[0], optchar);
	    optchar = '?';
	}

	/* 見つかったがオプション指定に ':' があるなら... */
	else if (ptr[1] == ':') {

	    /* 同じ argv 内に引数があるか */
	    if (*string)
		optarg = string;

	    /* options指定が ?:: ならば、同じ argv 内からしか見ない */
	    else if (ptr[2] == ':')
		optarg = NULL;

	    /* 次の引数にあるか */
	    else if (argv[next]) {

		/* ORDERING の必要があれば部分的に入れ換える */
		if (rotated) {
		    rotate_right (&argv[savepoint], next - savepoint + 1);
		    index = savepoint;
		}

		/* なければ... */
		else
		    index++;

		/* 次の引数を返す */
		optarg = argv[index];

	    }

	    /* なければ... */
	    else {
		ERR ("%s: option '-%c' requires an argument\n", argv[0], optchar);
		optchar = '?';
	    }

	}

      goback:

	/* 値を設定して戻る */
	rotated = 0;
	savepoint = 0;
	optind = index + 1;
	return optchar;

    }

    /* 普通の引数 */
    else {

      normal_arg:

	/* ORDERING する必要があるか？ */
	if (_getopt_no_ordering) {
	    init = 1;
	    optind = index;
	    return EOF;
	}

	/* 引数の位置を記憶し、次のオプションを調べる */
	else {
	    if (savepoint == 0)
		savepoint = index;
	    index++;
	    goto again;
	}

    }
}

/* EOF */
