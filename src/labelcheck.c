/* $Id: labelcheck.c,v 1.1 1996/11/07 08:03:46 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ラベルチェック
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>

#include "analyze.h"
#include "disasm.h"
#include "estruct.h"
#include "etc.h"	/* charout */
#include "global.h"
#include "label.h"	/* regist_label etc. */
#include "offset.h"	/* depend_address , nearadrs */


/* #define DEBUG */


USEOPTION   option_k, option_E, option_I;


static int Label_on_instruction_count = 0;
static int Fixed_count = 0;
static int Undefined_instruction_count = 0;


private void
oops_undefined (address pc0, address previous_pc, address nlabel)
{

    charout ('+');
    eprintf ("\n* WARNING! UNDEFINED INSTRUCTION FOUND.\n"
	     "%x - %x (%x)\n", pc0, nlabel, previous_pc);
    not_program (pc0, nlabel);
    Undefined_instruction_count++;
}


private address
labelshift (address previous_pc, address pc, address end, lblbuf* nadrs)
{
    address nlabel = nadrs->label;

    while (nlabel < pc) {
	charout ('!');
	nadrs->shift = nlabel - previous_pc;
	regist_label (previous_pc, PROLABEL);
#ifdef	DEBUG
	printf ("* FOUND at %x - %x - %x(diff %d) in opecode\n",
			previous_pc, nlabel, pc, nlabel - previous_pc);
#endif
	nadrs = Next (nadrs);		/* nadrs = next (nlabel + 1); */
	nlabel = nadrs->label;
    }

    if (nlabel >= end)	/* 泥縄 (^_^;) */
	return next_datlabel (previous_pc)->label;

    return end;
}


/*

  既に DATLABEL として登録済みかどうか調べる

*/
private boolean
datlabel (address adrs)
{
    lblbuf* ptr = search_label (adrs);

    return (ptr && isDATLABEL (ptr->mode)) ? TRUE : FALSE;
}


/*

  from から end まで逆アセンブルして命令の途中にアクセスしているところを捜す

*/
private address
search_change_operand (address from, address end)
{
    address nlabel, pc, previous_pc, pc0;
    disasm  code;
    boolean was_prog = FALSE;		/* 直前の領域がプログラムであったかどうか */

    pc = previous_pc = pc0 = from;

#ifdef	DEBUG
    printf ("search_ch_op(%x - %x)\n", from, end);
#endif

    while (end != (address)-1 && pc < end) {
	lblbuf* nadrs = next (pc + 1);
	address store = pc + Ofst;

	nlabel = nadrs->label;
	pc0 = pc;
	was_prog = TRUE;

	/* ２つのプログラムラベル間を逆アセンブル */
	while (pc < nlabel && pc < end) {
#ifdef	DEBUG
	    printf ("%x,%x\n", pc, nlabel);
#endif
	    previous_pc = pc;
	    store += dis (store, &code, &pc);
	    if (code.flag == UNDEF) {		/* 未定義命令なら警告 */
		oops_undefined (pc0, previous_pc, nlabel);
#ifdef	OSKDIS
		/* 未定義命令をデータにする */
		regist_label (previous_pc, DATLABEL | FORCE);
#endif	/* OSKDIS */
		pc = nlabel;
	    }
	}

	/* 命令の途中にアクセスしているなら */
	if (pc != nlabel) {
#ifdef	DEBUG
	    printf ("* label in instruction (%x, %x)\n", pc, nlabel);
#endif
	    Label_on_instruction_count++;
	    if (option_k
	     || (!option_E && code.size == BYTESIZE
		&& code.op1.eaadrs + 1 != nlabel
		&& code.op2.eaadrs + 1 != nlabel
		&& code.op3.eaadrs + 1 != nlabel
		&& code.op4.eaadrs + 1 != nlabel))
	    {
		charout ('*');
		Fixed_count++;
		not_program (pc0, nlabel);
		if (option_I)
		    eprintf ("\n命令の中を差すラベル(%x) -> "
			     "データ領域に変更しました(%x-%x)\n",
				nlabel, pc0, nlabel);
		pc = nlabel;
		was_prog = FALSE;
	    }
	    else {
		end = labelshift (previous_pc, pc, end, nadrs);
		if (option_I)
		    eprintf ("\n命令の中を差すラベル(%x -> %x+%x)\n",
				nlabel, previous_pc, nlabel - previous_pc);
	    }
	}
    }
    if (code.flag != RTSOP && code.flag != JMPOP && datlabel (pc) && was_prog) {
	charout ('-');
#ifdef	DEBUG
	printf ("datlabel in prolabel %x\n", pc);
#endif
	regist_label (pc, PROLABEL);
    }

    return end;
}


/*

  from から end までのデータ領域をチェック

*/
private void
search_change_data (address from, address end)
{
    address pc, pc0;
    lblbuf* nadrs;

    pc = pc0 = from;
    nadrs = next (pc);
#ifdef	DEBUG
    printf ("search_ch_dat(%x - %x)\n", from, end);
#endif

    while (end != (address)-1 && pc < end) {
	address dependadrs = nearadrs (pc);
	address nlabel;

	nadrs = Next (nadrs);		/* nadrs = next (pc + 1); */
	nlabel = nadrs->label;

	while (dependadrs != (address)-1 && dependadrs + 4 <= nlabel)
	    dependadrs = nearadrs (dependadrs + 1);

	while (dependadrs < nlabel && nlabel < dependadrs + 4) {
	    charout ('!');
	    nadrs->shift = nlabel - dependadrs;
	    regist_label (dependadrs, DATLABEL | UNKNOWN);
#ifdef	DEBUG
	    printf ("* FOUND at %x (diff %d) in data\n", dependadrs, nlabel - dependadrs);
#endif
	    nadrs = Next (nadrs);	/* nadrs = next (nlabel + 1); */
	    nlabel = nadrs->label;
	}
	pc = nlabel;
    }
}


extern void
search_operand_label (void)
{
#if 0
    address tmp;
#endif
    lblbuf* nadrs = next (BeginTEXT);
    address pc, pcend = nadrs->label;

    PCEND = Available_text_end;

    while (nadrs->label < Available_text_end) {
	charout ('.');
	nadrs = next_prolabel (pcend);

	pc    = nadrs->label;
	nadrs = next_datlabel (pc);
	pcend = nadrs->label;
#if 0
	if (tmp == pcend) {
	    nadrs = next_prolabel (pcend + 1);
	    pc    = nadrs->label;
	    nadrs = next_datlabel (pc);
	    pcend = nadrs->label;
	    printf ("ドロナワ処理をしました.\n");
	}
	tmp = pcend;
#endif

#ifdef	OSKDIS
	if (nadrs->label >= Available_text_end)
	    break;	/* 泥縄 */
#endif	/* OSKDIS */

	if (pcend != (address)-1)
	    pcend = search_change_operand (pc, pcend);
    }

    charout ('\n');
    if (Label_on_instruction_count) {
	if (option_q)
	    eputc ('\n');
	eprintf ("命令の中を指すラベル %d 個.\n", Label_on_instruction_count);
	if (Fixed_count)
	    eprintf ("%d 個の領域をデータ領域に変更しました.\n", Fixed_count);
    }

    nadrs = next (BeginTEXT);
    pcend = nadrs->label;

    while (nadrs->label < BeginBSS) {
	charout (':');
	nadrs = next_datlabel (pcend);
	pc = nadrs->label;
	nadrs = next_prolabel (pc);
	pcend = min (nadrs->label, BeginBSS);
#if 0
	if (tmp == pcend) {
	    nadrs = next_datlabel (pcend);
	    pc = nadrs->label;
	    nadrs = next_prolabel (pc + 1);
	    pcend = min (nadrs->label, BeginBSS);
	    printf ("ドロナワ処理2をしました.\n");
	}
	tmp = pcend;
#endif


#ifdef	DEBUG
	printf ("chk %x - %x\n", pc, pcend);
#endif
	if (pc < BeginBSS)
	    search_change_data (pc, pcend);
    }
    if (Undefined_instruction_count)	/* ラベルチェックに引っ掛かるとデータ消失 ? */
	analyze_data ();

    if (!option_q || !Label_on_instruction_count)
	eputc ('\n');
}


/* EOF */
