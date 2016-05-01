/*
 *
 *	ソースコードジェネレータ
 *	浮動小数点実数値文字列変換モジュールヘッダ
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	FPCONV_H
#define	FPCONV_H

#if !defined(__GNUC__) || (__GNUC__ < 2)
#error You lose. This file can be compiled only by GNU-C compiler version 2.
#endif


typedef union {
    struct {
	unsigned long hi;
	unsigned long lo;
    } ul;
    double d;
} quadword;

typedef union {
    struct {
	unsigned long hi;
	unsigned long mi;
	unsigned long lo;
    } ul;
    unsigned char uc[12];
} packed_decimal;


extern void	fpconv_s (char* buf, float* valp);
extern void	fpconv_d (char* buf, double* valp);
extern void	fpconv_x (char* buf, long double* valp);
extern void	fpconv_p (char* buf, packed_decimal* valp);
extern void	fpconv_q (char* buf, quadword* valp);

extern short	Inreal_flag;


#endif	/* FPCONV_H */

/* EOF */
