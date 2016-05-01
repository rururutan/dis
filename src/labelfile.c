/* $Id: labelfile.c,v 1.1 1996/11/07 08:03:48 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ラベルファイルモジュール
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <ctype.h>		/* toupper */
#include <stdio.h>
#include <string.h>

#include "analyze.h"
#include "estruct.h"
#include "etc.h"
#include "global.h"
#include "label.h"
#include "symbol.h"


/************************ ラベルファイルへの出力 ************************/


/*

  ラベルファイルのヘッダを書く

*/
static INLINE void
make_header (FILE* fp, char* filename)
{
    fprintf (fp, "*********************************************\n"
		 "*\n"
		 "*  Label file for %s\n"
		 "*\n"
		 "*\tDIS version %s\n"
		 "*\n"
		 "*********************************************\n",
		filename, Version);
}


/*

  ラベルファイルを出力

*/
extern void
make_labelfile (char* xfilename, char* labelfilename)
{
    FILE*   fp;
    lblbuf* nadrs;

    if ((fp = fopen (labelfilename, "w")) == NULL) {
	eprintf ("%s をオープンできませんでした.\n", labelfilename);
	return;
    }

    make_header (fp, xfilename);
    nadrs = next (BeginTEXT);

    while (nadrs->label != (address)-1) {
	int mode = (nadrs->mode & 0xff);

	fprintf (fp, "%06x\t", (unsigned int)nadrs->label);

	if (isPROLABEL (nadrs->mode))
	    fputc ('P', fp);
	else if (mode == RELTABLE || mode == RELLONGTABLE) {
	    fputc ('R', fp);
	    fputc (mode == RELTABLE ? 'W' : 'L', fp);
	} else {
	    static const unsigned char labelchar[ZTABLE + 1] = {
		'B', 'W', 'L', 'Q', 'U',	/* 整数 */
		'F', 'D', 'X', 'P', 'U',	/* 小数 */
		'S', 'U', 'U', 'Z'		/* その他 */
	    };

	    fputc ('D', fp);
	    fputc (mode > ZTABLE ? 'U' : labelchar[mode], fp);
	}
	if (nadrs->mode & FORCE)
	    fputc ('F', fp);

	{
	    symbol* symbolptr = symbol_search (nadrs->label);

	    if (symbolptr) {
		symlist* sym = &symbolptr->first;
		char space = '\t';

		do {
		    fprintf (fp, "%c%s", space, sym->sym);
		    space = ' ';
		    sym = sym->next;
		} while (sym);
	    }
	}

	fputc ('\n', fp);
	nadrs = Next (nadrs);		/* nadrs = next (nadrs->label + 1); */
    }

    fclose (fp);
}



/*********************** ラベルファイルからの入力 ***********************/


/*

  ラベルファイルの１行を解析する

  pass = 0, 大文字のみ判定
	 1, 小文字のみ判定

*/

static INLINE unsigned char*
skipspace (unsigned char* ptr)
{
    while (*ptr == ' ' || *ptr == '\t')
	ptr++;
    return ptr;
}

static INLINE unsigned char*
untilspace (unsigned char* ptr)
{
    while (*ptr && *ptr != ' ' && *ptr != '\t')
	ptr++;
    return ptr;
}

static INLINE int
get_line (char* linebuf, int line, int pass, address* adrs, char** symptrptr)
{
    address ad = (address) atox (linebuf);
    unsigned char* ptr = skipspace (untilspace ((unsigned char*) linebuf));
    opesize attr;

    if (Debug & BDEBUG)
	printf ("%x, %c\n", (unsigned int) ad, *ptr);

    if ((pass == 0 && islower (*ptr)) ||
	(pass == 1 && isupper (*ptr)))
	attr = -1;
    else {
	unsigned char c = *ptr++;

	attr = DATLABEL;
	switch (toupper (c)) {
	    case 'P':
		attr = PROLABEL;
		break;
	    case 'R':
		c = *ptr++;
		switch (toupper (c)) {
		    case 'W':
			attr |= RELTABLE;
			break;
		    case 'L':
			attr |= RELLONGTABLE;
			break;
		    default:
			goto er;
		}
		break;
	    case 'D':
		c = *ptr++;
		switch (toupper (c)) {
		    case 'B':	attr |= BYTESIZE;	break;
		    case 'W':	attr |= WORDSIZE;	break;
		    case 'L':	attr |= LONGSIZE;	break;
		    case 'Q':	attr |= QUADSIZE;	break;
		    case 'F':	attr |= SINGLESIZE;	break;
		    case 'D':	attr |= DOUBLESIZE;	break;
		    case 'X':	attr |= EXTENDSIZE;	break;
		    case 'P':	attr |= PACKEDSIZE;	break;
		    case 'S':	attr |= STRING;		break;
		    case 'Z':	attr |= ZTABLE;		break;
		    case 'U':	attr |= UNKNOWN;	break;

		    case 'R':
			attr |= RELTABLE;
			if (toupper (*ptr) == 'L') {
			    ptr++;
			    attr ^= (RELTABLE ^ RELLONGTABLE);
			}
			break;

		    default:
			goto er;
		}
		break;
	    default:
	    er:
		err ("\nlabelfile %d行 address %x : 不正な文字です.\n", line, ad);
	}
	if (toupper (*ptr) == 'F')
	    attr |= FORCE;
    }

    *symptrptr = (char*) skipspace (untilspace (ptr));
    *adrs = ad;
    return attr;
}


static INLINE void
work (address adrs, int attr, int pass, int line)
{
    if (pass == 0) {
	if (!regist_label (adrs, attr))
	    eprintf ("??? %d行 address %x\n", line, adrs);
    }
    else {
	if (isPROLABEL (attr)) {
	    if (Debug & BDEBUG)
		printf ("work:prog %x\n", (unsigned int) adrs);
	    if (!analyze (adrs, ANALYZE_IGNOREFAULT))
		eprintf ("\nAddress %x(labelfile Line %d)からはプログラムと見なせません.",
				adrs, line);
	}
	else {
	    switch (attr & 0xff) {
	    case RELTABLE :
		relative_table (adrs);
		break;
	    case RELLONGTABLE :
		relative_longtable (adrs);
		break;
	    case ZTABLE :
		z_table (adrs);
		break;
	    default:
		if (!regist_label (adrs, attr))
		    eprintf ("\n??? labelfile %d行 address %x", line, adrs);
		break;
	    }
	}
    }
}


/*

  ラベルファイルを読み込む

*/
extern void
read_labelfile (char* filename)
{
    FILE* fp;
    int pass;

    if ((fp = fopen (filename, "rt")) == NULL) {
	err ("\n%s をオープンできませんでした.\n", filename);
	return;
    }

    for (pass = 0; pass < 2; pass++) {
	int line;

	if (Debug & BDEBUG)
	    eprintf ("pass %d\n", pass);
	for (line = 1;; line++) {
	    char  linebuf[1024];
	    char* p;

	    if (fgets (linebuf, sizeof (linebuf), fp) == NULL)
		break;

	    /* 末尾の CR、LF を削除する */
	    for (p = linebuf + strlen (linebuf); linebuf < p; p--)
		if (p[-1] != (char)'\n' && p[-1] != (char)'\r')
		    break;
	    *p = '\0';

	    if (isxdigit ((unsigned char)linebuf[0])) {
		address adrs;
		char* symptr;
		int attr = get_line (linebuf, line, pass, &adrs, &symptr);

		if (attr != -1)
		    work (adrs, attr, pass, line);
		if (pass == 0) {
#if 0
		    del_symbol (adrs);
#endif
		    while (*symptr && *symptr != '*') {
			char* symend = (char*) untilspace ((unsigned char*) symptr);
			char* nextsym = (char*) skipspace ((unsigned char*) symend);
			char* label;

			*symend = '\0';
			label = Malloc (strlen (symptr) + 1);
			strcpy (label, symptr);
			add_symbol (adrs, 0, label);
			symptr = nextsym;
		    }
		}
	    }
	}
	if (pass == 0)
	    rewind (fp);
    }
    fclose (fp);
}


/* EOF */
