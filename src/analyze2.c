/* $Id: analyze2.c,v 1.1 1996/11/07 08:02:52 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	自動解析モジュール２
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>

#include "analyze.h"
#include "disasm.h"
#include "estruct.h"
#include "etc.h"	/* charout, peek[wl] */
#include "global.h"
#include "label.h"	/* regist_label etc. */
#include "offset.h"	/* depend_address , nearadrs */


USEOPTION	option_y, option_i, option_h;

/* #define DEBUG */


/*

  データエリア中のプログラム領域を判別する
  プログラムと認識したアドレスの数を返す

*/
extern int
research_data (void)
{
    lblbuf* lptr = next (BeginTEXT);
    address pc = lptr->label;
    lblmode nmode = lptr->mode;	/* 次の領域の属性 */
    lblmode mode = PROLABEL;	/* 現在の領域の属性 */
    int rc = 0;

#ifdef	DEBUG
    printf ("enter research_data\n");
#endif

    PCEND = Available_text_end;
    while (pc < Available_text_end) {
	address nlabel;

	mode = nmode;
	lptr = Next (lptr);		/* lptr = next (pc + 1) */
	nlabel = lptr->label;
	nmode = lptr->mode;

#ifdef	DEBUG
	printf ("tst %x(%x),%x\n", pc, mode, nlabel);
#endif
	if (!((long)pc & 1) && isDATLABEL (mode) && !(mode & FORCE)
	 && !depend_address (pc) && (mode & 0xff) == UNKNOWN && !isTABLE (mode)) {
	    address dummy = pc;
	    disasm code;

	    if (!option_y
	     || (!((ULONG)nlabel & 1) &&
		 ( (dis (nlabel - 2 + Ofst, &code, &dummy),
		       (code.flag != OTHER && code.flag != UNDEF))
		|| (dis (nlabel - 4 + Ofst, &code, &dummy) == 4
		    && (code.flag != OTHER && code.flag != UNDEF))
		|| (dis (nlabel - 6 + Ofst, &code, &dummy) == 6
		    && (code.flag != OTHER && code.flag != UNDEF))
	        ))
	    ) {
		if (analyze (pc, option_i ? ANALYZE_IGNOREFAULT : ANALYZE_NORMAL)) {
		    rc++;
#ifdef	DEBUG
		    printf ("* maybe program %5x - %5x\n", pc, nlabel);
#endif
		}
	    }
	}
	pc = nlabel;
    }

#ifdef	DEBUG
    printf ("exit research_data (rc=%d)\n", rc);
#endif

    return rc;
}

/*

  データ領域にあるアドレス依存のデータを登録する
  データ領域にある 0x4e75(rts) の次のアドレスを登録する

*/
extern void
analyze_data (void)
{
    address data_from, adrs;
    address data_to = BeginTEXT;

    do {
	data_from = next_datlabel (data_to)->label;
	data_to   = (address) min ((ULONG) next_prolabel (data_from)->label,
				   (ULONG) BeginBSS);
	charout ('#');

	/* アドレス依存のデータがあれば、そのアドレスを登録する */
	for (adrs = data_from;
	     (adrs = nearadrs (adrs)) < data_to && adrs < data_to;
	     adrs += 4) {
#ifdef	DEBUG
	    printf ("depend_address %x\n", (address) peekl (adrs + Ofst));
#endif
	    regist_label ((address) peekl (adrs + Ofst), DATLABEL | UNKNOWN);
	}

	/* rts の次のアドレスを登録する */
	if (option_h && data_from < Available_text_end) {
	    for (adrs = data_from + ((int)data_from & 1); adrs < data_to; adrs += 2) {
		if (peekw (adrs + Ofst) == 0x4e75) {
#ifdef	DEBUG
		    printf ("found 0x4e75 in %x\n", (int) adrs);
#endif
		    regist_label (adrs + 2, DATLABEL | UNKNOWN);
		}

#ifdef	OSKDIS
		/* link 命令に注目 */
		if ((peekw (adrs + Ofst) & 0xfff8) == 0x4e50)
		    regist_label (adrs, DATLABEL | UNKNOWN);
#endif	/* OSKDIS   */

	    }
	}

    } while (data_to < BeginBSS && adrs != (address)-1);
}


/*

  アドレステーブルを捜す

*/
extern void
search_adrs_table (void)
{
    lblbuf* lptr = next (BeginTEXT);
    address pc = lptr->label;
    lblmode nmode = lptr->mode;

    while (pc < (address) BeginBSS) {
	address nlabel;
	lblmode mode = nmode;

	lptr = next (pc + 1);
	nlabel = lptr->label;
	nmode = lptr->mode;
#ifdef DEBUG
	printf ("chk1(%x)", pc);
#endif
	if (isDATLABEL (mode)) {
	    address labeltop;
	    int count;

	    pc = (address) min((ULONG) nearadrs (pc), (ULONG) nlabel);
#ifdef DEBUG
	    printf ("chk2(%x)", pc);
#endif
	    labeltop = pc;
	    for (count = 0; depend_address (pc) && pc < nlabel; count++)
		pc += 4;
#ifdef DEBUG
	    printf ("count(%d)\n", count);
#endif
	    if (count >= 3) {
		int i;
#ifdef	DEBUG
		printf ("* found address table at %6x %d\n", pc - count * 4, count);
#endif
		for (i = 0; i < count; i++) {
		    address label = (address) peekl (labeltop + i * 4 + Ofst);
		    analyze (label, (option_i ? ANALYZE_IGNOREFAULT : ANALYZE_NORMAL));
		}
	    }
	}
	pc = nlabel;
    }
}




#ifdef	OSKDIS
/*

  ＩＤａｔａから初期化データを登録する

*/
extern void
analyze_idata (void)
{
    address offset = (address) (Top - (ULONG) Head.base + (ULONG) HeadOSK.idata);

    BeginDATA = BeginBSS + peekl (offset + 0);
    regist_label (BeginDATA, DATLABEL | UNKNOWN);
    regist_label (BeginDATA + peekl (offset + 4), DATLABEL | UNKNOWN);
}

/*

  ＩＲｅｆｓを解析

*/
static void
analyze_irefs_sub (int codelbl, ULONG* idatp, ULONG offset, int mode)
{
    if (peekl (idatp) <= offset && offset < (peekl (idatp) + peekl (&idatp[1]))) {
	address p = (address) &idatp[2];
	ULONG* w = (ULONG*) (p + offset - peekl (idatp));

	regist_label (peekl (w) + codelbl ? 0 : BeginBSS, mode);
    }
}

extern void
analyze_irefs (void)
{
    struct REFSTBL {
	UWORD base;
	UWORD cnt;
    } *offset;
    ULONG* idatp = (ULONG*) (Top - (ULONG) Head.base + (ULONG) HeadOSK.idata);

    offset = (struct REFSTBL*) (Top - (ULONG) Head.base + (ULONG) HeadOSK.irefs);

    while (ofset->cnt) {	/* コードポインタ */
	ULONG wk = BeginBSS + ((ULONG) ofset->base << 16);
	UWORD* p = (UWORD*) &ofset[1];
	int i;

	for (i = 0; i < ofset->cnt; i++) {
	    charout ('#');
#ifdef DEBUG
	    eprintf (":regist_label:%08x\n", BeginBSS + wk + peekw (p));
#endif
	    regist_label (wk + peekw (p), DATLABEL | LONGSIZE | CODEPTR | FORCE);
	    regist_label (wk + peekw (p) + 4, DATLABEL | UNKNOWN);
	    analyze_irefs_sub (TRUE, idatp, peekl (p), DATLABEL | UNKNOWN);
	    p++;
	}
	offset = (struct REFSTBL*) p;
    }
    offset++;
    while (ofset->cnt) {	/* データポインタ */
	ULONG wk = BeginBSS + ((ULONG) ofset->base << 16);
	UWORD* p = (UWORD*) &ofset[1];
	int i;

	for (i = 0; i < ofset->cnt; i++) {
	    charout ('#');
#ifdef DEBUG
	    eprintf (":regist_label:%08x\n", BeginBSS + wk + peekw (p));
#endif
	    regist_label (wk + peekw (p), DATLABEL | LONGSIZE | DATAPTR | FORCE);
	    regist_label (wk + peekw (p) + 4, DATLABEL | UNKNOWN);
	    analyze_irefs_sub (FALSE, idatp, peekl (p), DATLABEL | UNKNOWN);
	    p++;
	}
	offset = (struct REFSTBL*) p;
    }
}
#endif	/* OSKDIS   */

/* EOF */
