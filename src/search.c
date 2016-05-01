/* $Id: search.c,v 1.1 1996/11/07 08:03:58 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	文字列判定
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "estruct.h"
#include "etc.h"		/* charout, <jctype.h> */
#include "global.h"
#include "label.h"		/* regist_label etc. */
#include "offset.h"		/* depend_address, nearadrs */


/* #define DEBUG */


static INLINE void
foundstr (address pc, address ltemp)
{
    charout ('s');
#ifdef	DEBUG
    printf ("    * FOUND STRING AT %x - %x \n", pc, ltemp);
#endif
    regist_label (pc, DATLABEL | STRING);
}


/*

  文字列かどうかを調べる
  *newend = last string address

*/
private boolean
is_string (address from, address end, address* newend, int str_min)
{
    unsigned char* store;

#ifdef DEBUG
    printf ("  * is_string %x - %x\n", from, end);
#endif
    *newend = end;
    store = from + Ofst;

    while (store - Ofst < end) {
	unsigned char c = *store++;

	if (isprkana (c))			/* ANK 文字 */
	    ;
	else if (iskanji (c)) {			/* 全角文字 */
	    if (!iskanji2 (*store++))
		return FALSE;
	}
	else if (c == 0x80) {			/* 半角ひらがな */
	    c = *store++;
	    if (c < 0x86 || 0xfc < c)
		return FALSE;
	}
	else {
	    switch (c) {
	    case 0x07:	/* BEL */
	    case 0x09:	/* TAB */
	    case 0x0d:	/* CR  */
	    case 0x0a:	/* LF  */
	    case 0x1a:	/* EOF */
	    case 0x1b:	/* ESC */
		break;

	    case 0x00:	/* NUL */
		/* 10 億が .dc.b ';毀',0 にならないようにする */
		if (((int) store & 1) == 0
		 && peekl (store - sizeof (ULONG)) == 1000000000L
		 && (store - sizeof (ULONG) - Ofst) == from)
		    return FALSE;

		if (from + str_min < store - Ofst) {
		    while (store - Ofst < end && !*store)
			store++;
		    *newend = (address) min ((ULONG) (store - Ofst), (ULONG) end);
		    return TRUE;
		}
		return FALSE;
	    default:
		return FALSE;
	    }
	}
    }

    if (from + str_min <= store - Ofst) {
	*newend = (address) min ((ULONG) (store - Ofst), (ULONG) end);
	return TRUE;
    }
    return FALSE;
}


/*

  pc から nlabel までのデータ領域中の文字列をチェック

*/
private int
check_data_area (address pc, address nlabel, int str_min)
{
    int num_of_str = 0;

#ifdef	DEBUG
    printf ("* check_data_area %x - %x \n", pc, nlabel);
#endif

    while (pc < nlabel) {
	address ltemp, nlabel2;

	nlabel2 = ltemp = (address) min ((ULONG) nearadrs (pc), (ULONG) nlabel);
	while (pc < nlabel2) {
	    if (is_string (pc, ltemp, &ltemp, str_min)) {
		foundstr (pc, ltemp);
		num_of_str++;
		while (depend_address (ltemp) && ltemp + 4 <= nlabel2)
		    ltemp += 4;
		regist_label (ltemp, DATLABEL | UNKNOWN);
		pc = ltemp;
		ltemp = (address) min ((ULONG) nearadrs (pc), (ULONG) nlabel);
	    }
	    else
		pc = nlabel2;
	}
	while (depend_address (pc))
	    pc += 4;
    }

    return num_of_str;
}


/*

  文字列サーチ
  文字列として認識した数を返す

*/
extern int
search_string (int str_min)
{
    lblbuf* nadrs = next (BeginTEXT);
    lblmode nmode = nadrs->mode;
    address pc = nadrs->label;		/* 最初 */
    int num_of_str = 0;

    while (pc < BeginBSS) {
	lblmode mode = nmode;
	address nlabel;

	charout ('.');
	nadrs = Next (nadrs);		/* nadrs = next (pc + 1); */
	nlabel = nadrs->label;
	nmode = nadrs->mode;

	if (isDATLABEL (mode) && (mode & 0xff) == UNKNOWN)
	    num_of_str += check_data_area (pc, nlabel, str_min);

	pc = nlabel;
    }

    return num_of_str;
}


/* EOF */
