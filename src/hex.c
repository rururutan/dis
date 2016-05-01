/* $Id: hex.c,v 1.1 1996/11/07 08:03:40 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	16進変換 , String 関数
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <string.h>	/* strcpy */

#include "estruct.h"
#include "hex.h"


char Hex[16] = "0123456789abcdef";
int Zerosupress_mode = 0;

USEOPTION option_Z;


/* 手抜き ^^; */
extern char*
itod2 (char* buf, ULONG n)
{
    int i = n % 100;

    if (i / 10)
	*buf++ = (i / 10) + '0';
    *buf++ = (n % 10) + '0';
    *buf = '\0';
    return buf;
}


extern char*
itox (char* buf, ULONG n, int width)
{
    if (option_Z) {
	if (n == 0)
	    *buf++ = '0';
	else {
	    char tmp[8];
	    char* p = tmp + sizeof tmp;
	    int i;

	    do {
		/* 下位桁からテンポラリに変換 */
		*--p = Hex[n & 0xf];
		n >>= 4;
	    } while (n);
	    i = (tmp + sizeof tmp) - p;
	    do {
		/* 上位桁からバッファに転送 */
		*buf++ = *p++;
	    } while (--i > 0);
	}
    }

    else {
	char* p = buf += width;
	int i;

	for (i = width; --i >= 0;) {
	    /* 下位桁から変換 */
	    *--p = Hex[n & 0xf];
	    n >>= 4;
	}
    }

    *buf = '\0';
    return buf;
}


#define DEFINE_ITOX(func, width) \
extern char*			\
func (char *buf, ULONG n)	\
{				\
    char* p = buf += width;	\
    int i;			\
				\
    for (i = width; --i >= 0;) { \
	*--p = Hex[n & 0xf];	\
	n >>= 4;		\
    }				\
    *buf = '\0';		\
    return buf;			\
}

DEFINE_ITOX (itox8_without_0supress, 8);
DEFINE_ITOX (itox6_without_0supress, 6);
DEFINE_ITOX (itox4_without_0supress, 4);


/**** 以下の関数は GNU C Compiler 使用時には inline 展開されます. ****/

#ifndef __GNUC__

extern char*
strend (char *p)
{
    while (*p++)
	;
    return --p;
}


extern char*
itox2 (char* buf, ULONG n)
{
    if (!option_Z || n >= 0x10) {
	/* 10 の位 */
	*buf++ = Hex[(n >> 4) & 0xf];
    }
    /* 1 の位 */
    *buf++ = Hex[n & 0xf];
    *buf = '\0';
    return buf;
}


extern char*
itox2_without_0supress (char* buf, ULONG n)
{
    *buf++ = Hex[(n >> 4) & 0xf];
    *buf++ = Hex[n & 0xf];
    *buf = '\0';
    return buf;
}


extern char*
itoxd (char* buf, ULONG n, int width)
{
    if (Zerosupress_mode != 1 || n >= 10)
	*buf++ = '$';
    return (width == 2) ? itox2 (buf, n)
			: itox (buf, n, width);
}


extern char*
itox6 (char* buf, ULONG n)
{
    return itox (buf, n, (n >= 0x1000000) ? 8 : 6);
}


extern char*
itox6d (char* buf, ULONG n)
{
    if (Zerosupress_mode != 1 || n >= 10)
	*buf++ = '$';
    return itox6 (buf, n);
}

#endif	/* __GNUC__ */

/* EOF */
