/* $Id: disasmonly.c,v 1.1 1996/11/07 08:03:30 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	単なる逆アセンブルモジュール
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <string.h>	/* strcmp memcmp strlen */
#include <ctype.h>	/* tolower */

#include "disasm.h"
#include "estruct.h"
#include "etc.h"
#include "generate.h"
#include "global.h"
#include "hex.h"
#include "output.h"


USEOPTION   option_x, option_B;


static INLINE char*
strcpy2 (char* dst, char* src)
{
    while ((*dst++ = *src++) != 0)
	;
    return --dst;
}


private int
disasm1line (address pc, address pcend)
{
    char    buffer[256];
    address pc0 = pc, store0 = pc + Ofst;
    disasm  code;
    int   bytes;
    char* ptr;
    char* l;

    bytes = dis (store0, &code, &pc);
    modify_operand (&code);

    ptr = buffer;
    *ptr++ = '\t';

#ifdef	OSKDIS
#define OS9CALL(n, v, t)  (peekw (store0) == (n) && (v) < 0x100 && (l = (t)[(v)]))
    if (OS9CALL (0x4e40, code.op1.opval, OS9label))
	strcat (strcpy (ptr, "OS9\t"), l);
    else if (OS9CALL (0x4e4d, code.op2.opval, CIOlabel))
	strcat (strcpy (ptr, "TCALL\tCIO$Trap,"), l);
    else if (OS9CALL (0x4e4f, code.op2.opval, MATHlabel))
	strcat (strcpy (ptr, "TCALL\tT$Math,"), l);
#else
    if (*(UBYTE*)store0 == 0x70		/* moveq #imm,d0 + trap #15 なら */
     && peekw (store0 + 2) == 0x4e4f	/* IOCS コールにする		 */
     && IOCSlabel && (l = IOCSlabel[*(UBYTE*)(store0 + 1)]) != NULL
     && (pc < pcend)) {
	ptr = strcpy2 (ptr, IOCSCallName);
	*ptr++ = '\t';
	strcpy2 (ptr, l);
	bytes += 2;
	/* pc += 2; */
    }
#endif	/* OSKDIS */
    else {
	ptr = strcpy2 (ptr, code.opecode);
	if (code.size < NOTHING) {
	    *ptr++ = '.';
	    *ptr++ = opsize[code.size];
	}

	if (code.op1.operand[0]) {
	    *ptr++ = '\t';
	    ptr = strcpy2 (ptr, code.op1.operand);

	    if (code.op2.operand[0]) {
		if ((code.op2.ea != BitField) && (code.op2.ea != KFactor))
		    *ptr++ = ',';
		ptr = strcpy2 (ptr, code.op2.operand);

		if (code.op3.operand[0]) {
		    if ((code.op3.ea != BitField) && (code.op3.ea != KFactor))
			*ptr++ = ',';
		    ptr = strcpy2 (ptr, code.op3.operand);

		    if (code.op4.operand[0]) {
			if ((code.op4.ea != BitField) && (code.op4.ea != KFactor))
			    *ptr++ = ',';
			strcpy2 (ptr, code.op4.operand);
		    }
		}
	    }
	}
    }

    if (option_x)
	byteout_for_xoption (pc0, bytes, buffer);

    outputa (buffer);
    newline (pc0);

    /* rts、jmp、bra の直後に空行を出力する	    */
    /* ただし、-B オプションが無指定なら bra は除く */
    if ((code.opflags & FLAG_NEED_NULSTR)
	 && (option_B || (code.opecode[0] != 'b' && code.opecode[0] != 'B')))
	outputa (CR);

    return bytes;
}


/*

 単なる逆アセンブル

*/
extern void
disasmlist (char* xfilename, char* sfilename, time_t filedate)
{
    address pc = BeginTEXT;
#ifdef	OSKDIS
    address pcend = BeginBSS;
#else
    address pcend = BeginDATA;
#endif	/* OSKDIS */

    if (option_x)
	Atab += 2;

    init_output ();
    output_file_open (sfilename, 0);

    PCEND = pcend;
    while (pc < pcend) {
	char	adrs[8];
	itox6 (adrs, (ULONG) pc);
	outputa (adrs);
	pc += disasm1line (pc, pcend);
    }

    output_file_close ();
}


/* EOF */
