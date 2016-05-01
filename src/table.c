/* $Id: table.c,v 1.1 1996/10/24 04:27:40 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	テーブル処理モジュール
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <ctype.h>	/* isdigit, isxdigit */
#include <stdio.h>
#include <stdlib.h>	/* atoi */
#include <string.h>
#include <unistd.h>

#include "estruct.h"
#include "etc.h"
#include "global.h"
#include "hex.h"		/* strend */
#include "label.h"
#include "labelfile.h"
#include "offset.h"		/* depend_address */
#include "table.h"


USEOPTION	option_q;

extern int	Quiet_mode;


static table*	Table;
static int	TableCounter;
static int	Line_num;


/* table.loop mode */
#define TIMES_AUTOMATIC		0
#define TIMES_DECIDE_BY_BREAK	-1
#define eprintfq if( option_q && Quiet_mode < 1 ) eprintf


#ifdef DEBUG
#define debug(str)	printf (str)
#else
#define debug(str)
#endif


private void
interpret (table* table_ptr)
{
    extern int	yyparse (void);

    formula*	cur_expr = table_ptr->formulaptr;
    address	pc0 = 0;
    int		i, loop = 0;
    lblbuf*	lblptr = NULL;
    boolean	in_bss;
    address	limit;

    Eval_TableTop = Eval_PC = table_ptr->tabletop;
    ParseMode = PARSE_ANALYZING;

    in_bss = (BeginBSS <= Eval_PC) ? TRUE : FALSE;
    limit = in_bss ? Last : BeginBSS;

#if 0
    if (BeginBSS <= Eval_PC)
#else
    if (Last <= Eval_PC)	/* BSS にもテーブルを認める */
#endif
	return;

    do {
	for (i = 0; i < table_ptr->lines; i++) {
	    Eval_Count = 0;		/* First, Eval_Count is set in yyparse() */
	    do {
		Lexptr = cur_expr[i].expr;
		eprintfq ("[%d:%06x]:%s\n", loop + 1, Eval_PC, Lexptr);
		if (yyparse() == 1)
		    err ("Syntax error (%d 行)\n", cur_expr[i].line);

		cur_expr[i].id = Eval_ID;
		pc0 = Eval_PC;

		if (depend_address (Eval_PC)
			&& Eval_ID != LONGSIZE && Eval_ID != EVENID
#if 0
			&& Eval_ID != LONGID
#endif
			&& Eval_ID != BREAKID && Eval_ID != CRID) {
		    if (table_ptr->loop == TIMES_AUTOMATIC) {
			debug ("depend chk");
			goto tableend;
		    }
		    err ("アドレス依存部でロングワード指定されていません(%x)\n", Eval_PC);
		}

#define BSS_CHECK(idstr) ({ if (in_bss) goto bss_error; })
#define ODD_CHECK(size) ({ \
	if ((int)Eval_PC & 1) \
	    eprintf ("Warning: 奇数アドレス(%x)で%s指定されています.\n", Eval_PC, size); \
	})

		switch (Eval_ID) {
		case LONGSIZE:
		    ODD_CHECK ("ロングワード");
		    Eval_PC += 4;
		    break;
		case WORDSIZE:
		    ODD_CHECK ("ワード");
		    Eval_PC += 2;
		    break;
		case BYTESIZE:
		    Eval_PC++;
		    break;

		case SINGLESIZE:
		    ODD_CHECK ("単精度実数");
		    Eval_PC += 4;
		    break;
		case DOUBLESIZE:
		    ODD_CHECK ("倍精度実数");
		    Eval_PC += 8;
		    break;
		case EXTENDSIZE:
		    ODD_CHECK ("拡張精度実数");
		    Eval_PC += 12;
		    break;
		case PACKEDSIZE:
		    ODD_CHECK ("パックドデシマル");
		    Eval_PC += 12;
		    break;
#if 0
		case LONGID:
		    BSS_CHECK ("long");
		    ODD_CHECK ("ロングワード");
		    Eval_PC += Eval_Bytes*4;
		    break;
		case WORDID:
		    BSS_CHECK ("word");
		    ODD_CHECK ("ワード");
		    Eval_PC += Eval_Bytes*2;
		    break;
#endif
		case BYTEID:
		    BSS_CHECK ("byte");
		    Eval_PC += Eval_Bytes;
		    break;
		case ASCIIID:
		    BSS_CHECK ("ascii");
		    Eval_PC += Eval_Bytes;
		    break;
		case ASCIIZID:
		    BSS_CHECK ("asciiz");
		    while (PEEK_BYTE (Eval_PC))
			Eval_PC++;
		    break;
		case LASCIIID:
		    BSS_CHECK ("lascii");
		    Eval_PC += PEEK_BYTE (Eval_PC) + 1;
		    break;
		case EVENID:
		    if ((long)Eval_PC & 1)
			Eval_PC++;
		    break;
		case CRID:
		    break;
		case BREAKID:
		    if (Eval_Break) {
			debug ("break");
			loop++;
			goto tableend;
		    }
		    break;
		default:	/* reduce warning message */
		    break;
		}
#undef	ODD_CHECK
#undef	BSS_CHECK

		if (limit <= Eval_PC
		 || (table_ptr->loop == TIMES_AUTOMATIC
		     && ((lblptr = next (pc0 + 1)) == NULL
		      || Eval_PC >= lblptr->label))) {
		    loop++;
		    /*  _________ これでいい？ */
		    if (lblptr && Eval_PC > lblptr->label)
			Eval_PC = pc0;
		    debug ("exceeded next label");
		    goto tableend;
		}
#if 0
		if (Eval_ID != BREAKID && Eval_ID != CRID)
		    if (!regist_label (pc0, DATLABEL | Eval_ID))
			   eprintf ("??? address %x", pc0);
#endif
		Eval_Count--;
	    } while (Eval_Count > 0);
	}
	loop++;
    } while ((Eval_PC < limit
	&& (table_ptr->loop == TIMES_AUTOMATIC
	 && (lblptr = next (pc0 + 1)) != NULL && Eval_PC < lblptr->label)
	     )
	     || table_ptr->loop == TIMES_DECIDE_BY_BREAK
	     || (table_ptr->loop != TIMES_AUTOMATIC && loop < table_ptr->loop));

tableend:
    eprintfq ("Table は %x - %x (%d個)と判断しました\n",
		Eval_TableTop, Eval_PC, loop);

    if (!regist_label (Eval_PC, DATLABEL|ENDTABLE|UNKNOWN)) {
#if 0
	printf ("??? address %x\n", Eval_PC);
#endif
    }

    if (table_ptr->loop == TIMES_AUTOMATIC || table_ptr->loop == TIMES_DECIDE_BY_BREAK)
	table_ptr->loop = loop;			/* set implicit loop times */
    return;

bss_error:
    err ("BSS 内では使用できない識別子です(%d 行)\n", cur_expr[i].line);
}


private void
interpret_all (void)
{
    int i;

    for (i = 0; i < TableCounter; i++)
	interpret (&Table[i]);
}


/*

  テーブルの処理

*/
private void
tablejob (address tabletop, FILE* fp)
{
    formula* cur_expr;
    int formula_line;

    regist_label (tabletop, DATLABEL | TABLE | FORCE);

    /* テーブルバッファを一つ分確保 */
    Table = Realloc (Table, sizeof (table) * (TableCounter + 1));
    Table[TableCounter].tabletop = tabletop;

    cur_expr = 0;		/* for Realloc() */
    formula_line = 0;

    while (1) {
	char linebuf[1024];

	if (fgets (linebuf, sizeof linebuf, fp) == NULL)
	    err ("Table 中に EOF に達しました.\n");

	if (*(strend (linebuf) - 1) == '\n')
	    *(strend (linebuf) - 1) = '\0';
	Line_num++;
	if (linebuf[0] == '#' || linebuf[0] == '*')	/* comment */
	    continue;

	if (strncasecmp (linebuf, "end", 3) == 0) {
	    char* ptr;

	    for (ptr = linebuf + 3; *ptr == ' ' || *ptr == '\t'; ptr++)
		;
	    if (*ptr == '[') {
		for (ptr++; *ptr == ' ' || *ptr == '\t'; ptr++)
		    ;
		if (isdigit (*(unsigned char*)ptr)) {
		    Table[TableCounter].loop = atoi (ptr);
		    break;
		} else if (strncasecmp (ptr, "breakonly", 9) == 0) {
		    /* テーブル数を break のみで判断 */
		    Table[TableCounter].loop = TIMES_DECIDE_BY_BREAK;
		    break;
		} else if (*ptr == ']') {
		    Table[TableCounter].loop = TIMES_AUTOMATIC;
		    /* 指定されない -> 自動 */
		    break;
		}
		err ("Syntax error at end(%d 行)\n", Line_num);
	    } else
		Table[TableCounter].loop = 1;
	    break;
	} else {	/* if (strncasecmp (linebuf, "end", 3) != 0) */

	    /* 行バッファを一つ分確保して格納する */
	    cur_expr = Realloc (cur_expr, sizeof (formula) * (formula_line + 1));
	    cur_expr[formula_line].line = Line_num;
	    cur_expr[formula_line].expr =
		strcpy (Malloc (strlen (linebuf) + 1), linebuf);
	    formula_line++;
	}
    }

    Table[ TableCounter ].formulaptr = cur_expr;
    Table[ TableCounter ].lines      = formula_line;
    TableCounter++;
}


/*

  テーブル記述ファイルを読み込む

*/
extern void
read_tablefile (char* filename)
{
    char linebuf[256];
    FILE* fp = fopen (filename, "rt");

    if (!fp)
	err ("\n%s をオープンできませんでした.\n", filename);

    while (fgets (linebuf, sizeof linebuf, fp)) {
	unsigned char c = linebuf[0];

	Line_num++;
	if (c == '#' || c == '*' || c == ';')	/* comment */
	    continue;
	if (isxdigit (c)) {
	    address tabletop = (address) atox (linebuf);

	    eprintfq ("Table(%x)\n", tabletop);
	    tablejob (tabletop, fp);
	}
    }
    interpret_all ();
    fclose (fp);
}


/*

  テーブルのサーチ

*/
extern table*
search_table (address pc)
{
    int i;

    for (i = 0; i < TableCounter; i++) {
	if (Table[i].tabletop == pc)
	    return &Table[i];
    }
    return NULL;
}


/* EOF */
