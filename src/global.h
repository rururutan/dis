/* $Id: global.h,v 1.1 1996/10/24 04:27:46 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	大域変数ヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	GLOBAL_H
#define	GLOBAL_H

#include "estruct.h"


#ifndef NULL
#define NULL ((void *)0)
#endif

#ifdef	__GNUC__
    #define INLINE inline
    #undef  alloca
    #define alloca __builtin_alloca
#else
    #define INLINE
#endif


#ifdef PROFILE
#define private extern
#else
#define private static
#endif

extern xheader	Head;
#ifdef	OSKDIS
extern os9header HeadOSK;
#endif

extern ULONG	Top;		/* ポインター同士の加算は出来ないので unsigned int */
extern ULONG	Ofst;
extern address	BeginTEXT,	/* = Head.base */
		BeginDATA,	/* = Head.base + Head.text */
		BeginBSS,	/* = Head.base + Head.text + Head.data */
#ifndef	OSKDIS
		BeginSTACK,
#endif
		Last;		/* = Head.base + Head.text + Head.data + Head.bss */
extern address	Available_text_end;

extern int  Absolute;
#define NOT_ABSOLUTE	0
#define ABSOLUTE_ZFILE	1
#define ABSOLUTE_ZOPT	2

extern boolean	Exist_symbol;

extern const char Version[];
extern const char Date[];
#ifdef	OSKDIS
extern const char OSKEdition[];
#endif

#define USEOPTION	extern boolean	/* useful expression */
#define DIS_ENVNAME	"dis_opt"	/* 環境変数名 */

extern int  Debug;		/* Debug mode */
#define BDEBUG	1
#define BTRACE	2
#define BREASON	4
#define BLABEL	8


#endif	/* GLOBAL_H */

/* EOF */
