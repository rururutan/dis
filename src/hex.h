/* $Id: hex.h,v 1.1 1996/10/24 04:27:46 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	16進変換 , String 関数 ヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	HEX_H
#define	HEX_H

#include "global.h"


#define itox8(p, n)	itox (p, n, 8)
#define itox4(p, n)	itox (p, n, 4)

#define itox8d(p, n)	itoxd (p, n, 8)
#define itox4d(p, n)	itoxd (p, n, 4)
#define itox2d(p, n)	itoxd (p, n, 2)


extern char*	itod2 (char*, ULONG);	/* 手抜き */
extern char*	itox (char*, ULONG, int);
extern char*	itox8_without_0supress (char*, ULONG);
extern char*	itox6_without_0supress (char*, ULONG);
extern char*	itox4_without_0supress (char*, ULONG);

extern char Hex[16];
extern int  Zerosupress_mode;


#ifdef	__GNUC__

INLINE static char*
strend (char *p)
{
    while (*p++)
	;
    return --p;
}

INLINE static char*
itox2 (char* buf, ULONG n)
{
    USEOPTION option_Z;		/* extern boolean option_Z; */

    if (!option_Z || n >= 0x10) {
	/* 10 の位 */
	*buf++ = Hex[(n >> 4) & 0xf];
    }
    /* 1 の位 */
    *buf++ = Hex[n & 0xf];
    *buf = '\0';
    return buf;
}

INLINE static char*
itox2_without_0supress (char *buf, ULONG n)
{
    *buf++ = Hex[(n >> 4) & 0xf];
    *buf++ = Hex[n & 0xf];
    *buf = '\0';
    return buf;
}

INLINE static char*
itoxd (char* buf, ULONG n, int width)
{
    if (Zerosupress_mode != 1 || n >= 10)
	*buf++ = '$';
    return (width == 2) ? itox2 (buf, n)
			: itox (buf, n, width);
}

INLINE static char*
itox6 (char* buf, ULONG n)
{
    return itox (buf, n, (n >= 0x1000000) ? 8 : 6);
}

INLINE static char*
itox6d (char* buf, ULONG n)
{
    if (Zerosupress_mode != 1 || n >= 10)
	*buf++ = '$';
    return itox6 (buf, n);
}

#else

extern char*	strend (char*);
extern char*	itox2 (char*, ULONG);
extern char*	itox2_without_0supress (char*, ULONG);
extern char*	itoxd (char*, ULONG, int);
extern char*	itox6 (char*, ULONG);
extern char*	itox6d (char*, ULONG);

#endif	/* !__GNUC__ */


#endif	/* HEX_H */
