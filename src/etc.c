/* $Id: etc.c,v 1.1 1996/11/07 08:03:32 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	雑用ルーチン
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <ctype.h>	/* isxdigit isdigit */
#include <stdlib.h>	/* exit(), EXIT_FAILURE */
#include <stdarg.h>

#include "estruct.h"
#include "etc.h"
#include "global.h"
#include "label.h"
#include "offset.h"
#include "symbol.h"


/* function in main.c */
extern void free_load_buffer (void);
extern void print_title (void);


extern ULONG
atox (char* p)
{
    ULONG val = 0;

    if (p[0] == (char)'$')
	p++;
    /* 0x... なら頭の 0x を取り除く */
    else if (p[0] == (char)'0' && p[1] == (char)'x' && p[2])
	p += 2;

    while (isxdigit (*(unsigned char*)p)) {
	unsigned char c = *p++;
	val <<= 4;
	if (isdigit (c))
	    val += c - '0';
	else
	    val += tolower (c) - 'a' + 10;
    }
    return val;
}


/* フォーマット文字列を表示してエラー終了する. */
extern void
err (const char* fmt, ...)
{
    va_list ap;

    print_title ();
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);

    free_labelbuf ();
    free_relocate_buffer ();
    free_symbuf ();
    free_load_buffer ();

    exit (EXIT_FAILURE);
}


/*

  safe malloc

*/
extern void*
Malloc (ULONG byte)
{
    void* rc;

    if (byte == 0)
	byte = 1;
    if ((rc = malloc (byte)) == NULL)
	err ("ヒープメモリが不足しています.\n");

    return rc;
}


#ifndef Mfree
/*

  safe mfree

  free() は NULL を渡した場合は何もせずに正常終了するので、通常は
  #define Mfree(ptr) free(ptr)
  としておきます.  

*/
extern void
Mfree (void* ptr)
{
    if (ptr)
	free (ptr);
}
#endif


/*

  safe realloc

*/
extern void*
Realloc (void* ptr, int size)
{
    void* rc;
    
    if ((rc = realloc (ptr, size)) == NULL)
	err ("ヒープメモリが不足しています.\n");
    return rc;
}


#ifndef	__LIBC__
extern int
eprintf (const char* fmt, ...)
{
    int cnt;
    va_list ap;

    va_start (ap, fmt);
    cnt = vfprintf (stderr, fmt, ap);
    va_end (ap);
    return cnt;
}
#endif

/* #define eputc (c) fputc (c, stderr) */
extern int
eputc (int c)
{
    return fputc (c, stderr);
}


#ifndef	__GNUC__
extern ULONG
min (ULONG a, ULONG b)
{
    return a < b ? a : b;
}

extern ULONG
max (ULONG a, ULONG b);
{
    return a > b ? a : b;
}
#endif


#ifndef HAVE_STRUPR
extern char*
strupr (char* str)
{
    unsigned char* p;

    for (p = (unsigned char*) str; *p; p++) {
	if (islower (*p))
	    *p = toupper (*p);
    }
    return str;
}
#endif


/* EOF */
