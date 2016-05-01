/*
 *
 *	ソースコードジェネレータ
 *	浮動小数点実数値文字列変換モジュール
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

/* System headers */
#include <stdio.h>	/* sprintf() */
#include <string.h>	/* strcpy() */

/* Headers */
#include "estruct.h"	/* enum boolean */
#include "fpconv.h"
#include "hex.h"	/* itox8_without_0supress() */

/* External functions */
#ifdef	HAVE_STRTOX
extern	long double strtox (const char* ptr);
#endif


/* External variables */
short	Inreal_flag = FALSE;


/* Functions */
extern void
fpconv_s (char* buf, float* valp)
{
    unsigned short e = *(unsigned short*)valp & 0x7f80;
    unsigned long m = *(unsigned long*)valp & 0x007fffff;

    /* ±0.0を先に処理 */
    if (!Inreal_flag && e == 0 && m == 0) {
	strcpy (buf, (*(char*)valp < 0) ? "0f-0" : "0f0");
	return;
    }

    if (Inreal_flag || e == 0 || e == 0x7f80) {		/* 非正規化数, 無限大、非数 */
	*buf++ = '!';					/* 特殊な値なら内部表現で出力 */
	itox8_without_0supress (buf, *(long*)valp);
    } else {
	sprintf (buf, "0f%.30g", *valp);
    }
}


/* Functions */
extern void
fpconv_d (char* buf, double* valp)
{
    unsigned short e = *(unsigned short*)valp & 0x7ff0;
    unsigned long mh = *(unsigned long*)valp & 0x000fffff;
    unsigned long ml = *(unsigned long*)valp;

    /* ±0.0を先に処理 */
    if (!Inreal_flag && e == 0 && (mh | ml) == 0) {
	strcpy (buf, (*(char*)valp < 0) ? "0f-0" : "0f0");
	return;
    }

    if (Inreal_flag || e == 0 || e == 0x7ff0) {		/* 非正規化数, 無限大、非数 */
	long* p = (long*)valp;				/* 特殊な値なら内部表現で出力 */

	*buf++ = '!';
	buf = itox8_without_0supress (buf, *p++);
	*buf++ = '_';
	(void)itox8_without_0supress (buf, *p);
    } else {
	sprintf (buf, "0f%.30g", *valp);
    }
}


/* File scope functions */
static void
fpconv_x_inreal (char* buf, long double* valp)
{
    unsigned long* p = (unsigned long*)valp;

    *buf++ = '!';
    buf = itox8_without_0supress (buf, *p++);
    *buf++ = '_';
    buf = itox8_without_0supress (buf, *p++);
    *buf++ = '_';
    (void)itox8_without_0supress (buf, *p++);
}

/* Functions */
extern void
fpconv_x (char* buf, long double* valp)
#ifdef NO_PRINTF_LDBL
{
    /* 常に内部表現で出力 */
    fpconv_x_inreal (buf, valp);
}
#else /* !NO_PRINTF_LDBL */
{
    unsigned short e = *(unsigned short*)valp & 0x7fff;
    unsigned long mh = ((unsigned long*)valp)[0];
    unsigned long ml = ((unsigned long*)valp)[1];

    /* ±0.0を先に処理 */
    if (!Inreal_flag && e == 0 && (mh | ml) == 0) {
	strcpy (buf, (*(char*)valp < 0) ? "0f-0" : "0f0");
	return;
    }

    if (Inreal_flag || e == 0 || e == 0x7fff		/* 非正規化数, 無限大、非数 */
     || ((unsigned short*)valp)[1]			/* 未使用ビットがセット */
     || ((char*)valp)[4] >= 0				/* 整数ビットが0 */
#ifndef	HAVE_STRTOX
     || e <= 64						/* ライブラリに依存 */
#endif
    ) {
	fpconv_x_inreal (buf, valp);			/* 特殊な値なら内部表現で出力 */
    } else {
	sprintf (buf, "0f%.30Lg", *valp);

#ifdef	HAVE_STRTOX
	/* 指数が正なら多分正しく変換できている筈 */
	if (e >= 0x3fff)
	    return;

	/* 正しく変換できなかったら、内部表現で出力しなおす */
	if (strtox (buf + 2) != *valp)
	    fpconv_x_inreal (buf, valp);
#endif
    }
}
#endif	/* !NO_PRINTF_LDBL */


/* File scope functions */
static boolean
is_normalized_p (packed_decimal* valp)
{

    if ((valp->ul.hi<<1 | valp->ul.mi | valp->ul.lo) == 0)
	return TRUE;					/* ±0.0 */

    if ( ((valp->ul.hi & 0x7fff0000) == 0x7fff0000)	/* 非数,無限大 */
      || ((valp->ul.hi & 0x3000fff0) != 0)		/* 未使用ビットがセット */
#if 0
      || ((valp->ul.hi & 0x4fff0000) == 0x4fff0000)	/* 非正規化数 */
#endif
      || ((valp->ul.hi & 0x0000000f) > 9) )		/* BCD(整数桁)の値が異常 */
	return FALSE;

    {
	int i;
	unsigned char *ptr = &valp->uc[4];
	for (i = 0; i < 8; i++) {
	    unsigned char c = *ptr++;
	    if ((c > (unsigned char)0x99) || ((c & 0x0f) > (unsigned char)0x09))
		return FALSE;				/* BCD(小数桁)の値が異常 */
	}
    }

   return TRUE;
}


/* Functions */
extern void
fpconv_p (char* buf, packed_decimal* valp)
{

    if (Inreal_flag || !is_normalized_p (valp)) {
	long* p = (long*)valp;				/* 特殊な値なら内部表現で出力 */

	*buf++ = '!';
	buf = itox8_without_0supress (buf, *p++);
	*buf++ = '_';
	buf = itox8_without_0supress (buf, *p++);
	*buf++ = '_';
	(void)itox8_without_0supress (buf, *p);

    } else {
	*buf++ = '0';
	*buf++ = 'f';
	if (valp->uc[0] & 0x80)
	    *buf++ = '-';

	/* 仮数の整数部 */
	*buf++ = '0' + (valp->ul.hi & 0x0000000f);
	*buf++ = '.';

	/* 仮数の小数部 */
	{
	    int i;
	    unsigned char *ptr = &valp->uc[4];

	    for (i = 0; i < 8; i++) {
		unsigned char c = *ptr++;
		*buf++ = '0' + (c >> 4);
		*buf++ = '0' + (c & 0x0f);
	    }
	}

	/* 末尾の'0'を削除 */
	while (*--buf == (char)'0')
	    ;
	if (*buf != (char)'.')
	    buf++;

	/* 指数 */
	{
	    int expo = (valp->uc[0] & 0x0f)*100
		     + (valp->uc[1] >> 4)*10
		     + (valp->uc[1] & 0x0f);

	    if ((expo == 0) && !(valp->uc[0] & 0x40)) {
		/* "e+0"は省略する */
		*buf = '\0';
	    } else {
		*buf++ = 'e';
		*buf++ = (valp->uc[0] & 0x40) ? '-' : '+';
		buf += (expo >= 100) + (expo >= 10) + 1;
		*buf = '\0';
		do {
		    *--buf = '0' + (expo % 10);
		    expo /= 10;
		} while (expo);
	    }
	}
    }
}


/* Functions */
extern void
fpconv_q (char* buf, quadword* valp)
{
    *buf++ = '!';
    buf = itox8_without_0supress (buf, valp->ul.hi);
    *buf++ = '_';
    (void)itox8_without_0supress (buf, valp->ul.lo);
}


/* EOF */
