/* $Id: eval.y,v 1.1 1994/07/09 19:54:20 ryo Exp $
 *
 *	ソースコードジェネレータ
 *	ラベルファイルパーサ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

%{
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disasm.h"
#include "estruct.h"
#include "etc.h"
#include "fpconv.h"
#include "generate.h"	/* symset */
#include "global.h"
#include "hex.h"	/* strend */
#include "label.h"
#include "labelfile.h"
#include "offset.h"	/* depend_address */
#include "table.h"


/*
#define	YYDEBUG	1
*/
#ifdef SELF
extern int	yydebug;
#endif

parse_mode	ParseMode;
char*		Lexptr;
int		Eval_Value;
address		Eval_TableTop;
opesize		Eval_ID;
address		Eval_PC;
int		Eval_Bytes;
char		Eval_ResultString[256];
int		Eval_Break;
int		Eval_Count;

typedef struct {
    boolean	isvalue;
    boolean	registed;
    address	value;
    char*	bufptr;
} expbuf;

static int		yylex (void);
static void		yyerror (char*);
static void		tabledesc (int, opesize);
static unsigned long	peek (address, opesize);
static unsigned long	extend (unsigned long, opesize);
static expbuf*		storestr (char*);
static expbuf*		storeexp (address, int);
static expbuf*		store (expbuf*);
static void		breakjob (unsigned long);
static void		itoxd_by_size (char*, ULONG, opesize);


#define	EXPSTACKSIZE	32
static expbuf*		ExpStack[EXPSTACKSIZE];
static int		ExpStackCounter = 0;


%}

/*

	Begin Grammer

*/
%union {
	unsigned long	val;
	char*		str;
	expbuf*		exp;
}
%token	<str>		_STRING
%token	<val>		SIZEID FSIZEID
%token	<val>		NUMBER
%token	<val>		PEEKB PEEKW PEEKL BREAK EXTW EXTL
%type	<exp>		str_or_exp	/* exprstring */
%type	<val>		sizeid fsizeid
%type	<val>		tabledesc
%type	<val>		expr logical_AND_expr equality_expr relational_expr
%type	<val>		additive_expr mul_expr factor element
%left	'<' LE '>'	GE EQUEQU NOTEQU
%left	'(' ')' '{' '}'
%left	','
%left	OROR
%left	ANDAND
%left	'+' '-'
%left	'*' '/'
%left	'!'
%left	UNARYMINUS

%%

tabledesc : BREAK expr				{ breakjob ($2); }
	| sizeid				{ tabledesc ( 1, $1); }
	| sizeid '[' expr ']'			{ tabledesc ($3, $1); }
	| sizeid exprstring			{ tabledesc ( 1, $1); }
	| sizeid '[' expr ']' exprstring	{ tabledesc ($3, $1); }
	| fsizeid				{ tabledesc ( 1, $1); }
	| fsizeid '[' expr ']'			{ tabledesc ($3, $1); }
	;

sizeid : SIZEID					{ $$ = $1; }
	| '@' SIZEID				{ $$ = $2; }
	;

fsizeid : FSIZEID				{ $$ = $1; }
	| '@' FSIZEID				{ $$ = $2; }
	;

exprstring : str_or_exp				{ store ($1); }
	| exprstring ',' str_or_exp 		{ store ($3); }
	;

str_or_exp : _STRING				{ $$ = storestr ($1); }
	| expr					{ $$ = storeexp ((address) $1, 0); }
	| '{' expr '}'				{ $$ = storeexp ((address) $2, 1); }
	;

expr : logical_AND_expr				{ $$ = $1; }
	| expr OROR logical_AND_expr		{ $$ = $1 || $3; }
	;

logical_AND_expr : equality_expr		{ $$ = $1; }
	| logical_AND_expr ANDAND equality_expr	{ $$ = $1 && $3; }
	;

equality_expr : relational_expr			{ $$ = $1; }
	| equality_expr EQUEQU relational_expr	{ $$ = ($1 == $3); }
	| equality_expr NOTEQU relational_expr	{ $$ = ($1 != $3); }
	;

relational_expr : additive_expr			{ $$ = $1; }
	| relational_expr '<' additive_expr	{ $$ = ($1 <  $3); }
	| relational_expr LE additive_expr	{ $$ = ($1 <= $3); }
	| relational_expr '>' additive_expr	{ $$ = ($1 >  $3); }
	| relational_expr GE additive_expr	{ $$ = ($1 >= $3); }
	;

additive_expr : mul_expr			{ $$ = $1; }
	| additive_expr '+' mul_expr		{ $$ = $1 + $3; }
	| additive_expr '-' mul_expr		{ $$ = $1 - $3; }
	;

mul_expr : factor				{ $$ = $1; }
	| mul_expr '*' factor			{ $$ = $1 * $3; }
	| mul_expr '/' factor			{ $$ = $1 / $3; }
	| mul_expr '%' factor			{ $$ = $1 % $3; }
	;

factor : '-' factor %prec UNARYMINUS	 	{ $$ = - $2; }
	| '!' factor				{ $$ = ! $2; }
	| element				{ $$ = $1; }
	;

element : '(' expr ')'				{ $$ = $2; }
	| NUMBER				{ $$ = $1; }
	| PEEKB '(' expr ')'	{ $$ = peek ((address) $3, BYTESIZE); }
	| PEEKW '(' expr ')'	{ $$ = peek ((address) $3, WORDSIZE); }
	| PEEKL '(' expr ')'	{ $$ = peek ((address) $3, LONGSIZE); }
	| EXTW '(' expr ')'	{ $$ = extend ((unsigned) $3, WORDSIZE); }
	| EXTL '(' expr ')'	{ $$ = extend ((unsigned) $3, LONGSIZE); }
	;
%%

/*

	End of Grammer

*/

static int
get_token_id (const char* token)
{

    /* SIZEID, FSIZEID */
    {
	struct idpair {
	    char idstr[8];
	    int  idnum;
	} idbuf[] = {
	    { "dc.b"  , BYTESIZE },
	    { "dc.w"  , WORDSIZE },
	    { "dc.l"  , LONGSIZE },
	    { "dc"    , WORDSIZE },
	    { "byte"  , BYTEID },
	    { "even"  , EVENID },
	    { "cr"    , CRID },
	    { "asciz" , ASCIIZID },
	    { "asciiz", ASCIIZID },
	    { "ascii" , ASCIIID },
	    { "lascii", LASCIIID },
#if 0
	    { "long"  , LONGID },
	    { "word"  , WORDID },
#endif
	    { "dc.s"  , SINGLESIZE },
	    { "dc.d"  , DOUBLESIZE },
	    { "dc.x"  , EXTENDSIZE },
	    { "dc.p"  , PACKEDSIZE },
	    { ""      , 0 }		/* end of list */
	};
	struct idpair* idptr;
	const char* t = token;

	if (*t == (char)'.')
	    t++;			/* 先頭のピリオドは無視 */
	for (idptr = idbuf; idptr->idstr[0]; idptr++) {
	    if (strcasecmp (t, idptr->idstr) == 0) {
		yylval.val = (unsigned long) idptr->idnum;
		return (SINGLESIZE <= idptr->idnum) ? FSIZEID : SIZEID;
	    }
	}
    }

    /* NUMBER */
    if (strcasecmp (token, "tabletop") == 0) {
	yylval.val = (unsigned long) Eval_TableTop;
	return NUMBER;
    }
    if (strcasecmp (token, "pc") == 0) {
	yylval.val = (unsigned long) Eval_PC;
	return NUMBER;
    }

    /* BREAK, PEEK*, EXT* */
    if (strcasecmp (token, "break")  == 0)	return BREAK;
    if (strcasecmp (token, "peek.b") == 0)	return PEEKB;
    if (strcasecmp (token, "peek.w") == 0)	return PEEKW;
    if (strcasecmp (token, "peek.l") == 0)	return PEEKL;
    if (strcasecmp (token, "ext.w")  == 0)	return EXTW;
    if (strcasecmp (token, "ext.l")  == 0)	return EXTL;

    /* トークンではなかった */
    return -1;
}


static int
yylex (void)
{
    unsigned char c;

    while ((c = *Lexptr++) == ' ' || c == '\t' || c == '\n')
	;
    if (c == '\0')
	return 0;

    /* 16 進数 */
    if (c == '$' || (c == '0' && (char) tolower (*(unsigned char*)Lexptr) == 'x')) {
	char* p;

	if (c == '0')
	    Lexptr++;
	p = Lexptr;
	yylval.val = 0;
	while ((c = *Lexptr++), isxdigit (c))
	    yylval.val = (yylval.val << 4)
		       + (toupper (c) >= 'A' ? toupper (c) - 'A' + 10 : c - '0');
	return (--Lexptr == p) ? 1 : NUMBER;
    }
    /* 10 進数 */
    else if (isdigit (c)) {
	yylval.val = c - '0';
	while ((c = *Lexptr++), isdigit (c))
	    yylval.val = yylval.val * 10 + c - '0';
	Lexptr--;
	return NUMBER;
    }

    if (c == '"') {
	char* term;

	if ((term = strchr (Lexptr, '"')) == NULL)
	    return 1;
	yylval.str = Malloc (term - Lexptr + 1);
	strncpy (yylval.str, Lexptr, term - Lexptr);
	yylval.str[term - Lexptr] = '\0';
	Lexptr = term + 1;
	return _STRING;
    }
    if (c == '<') {
	if (*Lexptr == '=') {
	    Lexptr++;
	    return LE;
	}
	return c;
    }
    if (c == '>') {
	if (*Lexptr == '=') {
	    Lexptr++;
	    return GE;
	}
	return c;
    }
    if (c == '|') {
	if (*Lexptr == '|') {
	    Lexptr++;
	    return OROR;
	}
	return c;
    }
    if (c == '&') {
	if (*Lexptr == '&') {
	    Lexptr++;
	    return ANDAND;
	}
	return c;
    }
    if (c == '=') {
	if (*Lexptr == '=') {
	    Lexptr++;
	    return EQUEQU;
	}
	return c;
    }
    if (c == '!') {
	if (*Lexptr == '=') {
	    Lexptr++;
	    return NOTEQU;
	}
	return c;
    }

    if (isalpha (c) || c == '.') {
	char* token = --Lexptr;
	unsigned char c;
	char next;
	int ret;

	while ((c = *Lexptr++), isalnum (c) || c == '.')
	    ;
	next = *--Lexptr;
	*Lexptr = '\0';			/* トークンを切り出す */
	ret = get_token_id (token);
	*Lexptr = next;

	if (ret >= 0)
	    return ret;

	/* トークンではなかった */
	Lexptr = token + 1;
    }

    return c;
}


static void
yyerror (char* s)
{
    eprintf ("%s\n", s);
}


static void
fpconv_by_size (char* buf, address ptr, opesize size)
{
    unsigned long val[3];

    val[0] = PEEK_LONG (ptr);
    if (size == SINGLESIZE) {
	fpconv_s (buf, (float*) (void*) &val);
	return;
    }

    ptr += 4;
    val[1] = PEEK_LONG (ptr);
    if (size == DOUBLESIZE) {
	fpconv_d (buf, (double*) (void*) &val);
	return;
    }

    ptr += 4;
    val[2] = PEEK_LONG (ptr);
    if (size == EXTENDSIZE)
	fpconv_x (buf, (long double*) (void*) &val);
    else /* if (size == PACKEDSIZE) */
	fpconv_p (buf, (packed_decimal*) &val);
}


static void
tabledesc (int num, opesize id)
{
    int i;

    Eval_ID = id;
    if (Eval_Count == 0) {		/* easy trick ? */
	if (id == ASCIIID || id == BYTEID
#if 0
	 || id == WORDID || id == LONGID
#endif
	) {
	    Eval_Count = 1;
	    Eval_Bytes = num;
	} else
	    Eval_Count = num;
    }


    if (ExpStackCounter == 0 && ParseMode == PARSE_GENERATING) {
	char* bufptr = strend (Eval_ResultString);

	switch (id) {
	case LONGSIZE:
	    if (depend_address (Eval_PC))
		make_proper_symbol (bufptr, (address) PEEK_LONG (Eval_PC));
	    else
		itoxd_by_size (bufptr, PEEK_LONG (Eval_PC), id);
	    break;
	case WORDSIZE:
	    itoxd_by_size (bufptr, PEEK_WORD (Eval_PC), id);
	    break;
	case BYTESIZE:
	    itoxd_by_size (bufptr, PEEK_BYTE (Eval_PC), id);
	    break;

	case SINGLESIZE:
	case DOUBLESIZE:
	case EXTENDSIZE:
	case PACKEDSIZE:
	    fpconv_by_size (bufptr, Eval_PC, id);
	    break;

	default:	/* reduce warning message */
	    break;
	}
    }
    for (i = 0; i < ExpStackCounter; i++) {
	switch (ParseMode) {
	case PARSE_ANALYZING:
	    if (ExpStack[i]->isvalue == FALSE)
		free (ExpStack[i]->bufptr);	/* 文字列バッファを解放 */
	    free (ExpStack[i]);
	    break;
	case PARSE_GENERATING:
	    if (ExpStack[i]->isvalue) {
		char* bufptr = strend (Eval_ResultString);
		address val = ExpStack[i]->value;

		if (ExpStack[i]->registed)
		    make_proper_symbol (bufptr, val);
		else
		    itoxd_by_size (bufptr, (ULONG) val, Eval_ID);
	    } else {
		strcat (Eval_ResultString, ExpStack[i]->bufptr);
		free (ExpStack[i]->bufptr);
	    }
	    free (ExpStack[i]);
	    break;
	}
    }
    ExpStackCounter = 0;
}


static expbuf*
storestr (char* s)
{
    expbuf* exp = Malloc (sizeof (expbuf));

    exp->bufptr = s;
    exp->isvalue = FALSE;

    return exp;
}


static expbuf*
storeexp (address v, int mode)
{
    expbuf* exp = Malloc (sizeof (expbuf));

    exp->value = v;
    exp->isvalue = TRUE;

    if (mode == 1) {
	exp->registed = TRUE;
	if (ParseMode == PARSE_ANALYZING
	 && !regist_label (v, DATLABEL | UNKNOWN))
	    printf ("??? address %x\n", (unsigned int) v);
    } else
	exp->registed = FALSE;

    return exp;
}


static expbuf*
store (expbuf* exp)
{

#if 0	/* debug */
    if (exp->isvalue == TRUE)
	printf ("値 = %d\n", exp->value);
    else
	printf ("文字列 = %s\n", exp->bufptr);
#endif

    if (ExpStackCounter == EXPSTACKSIZE)
	err ("式評価スタックがあふれました\n");
    ExpStack[ExpStackCounter++] = exp;

    return exp;
}


static void
itoxd_by_size (char* buf, ULONG n, opesize size)
{
    itoxd (buf, n, (size == LONGSIZE) ? 8 :
		   (size == WORDSIZE) ? 4 : 2);
}


static void
peek_warning (address adrs, int size)
{
    if ((int)adrs & 1)
	eprintf ("Warning: peek.%c の引数が奇数アドレス(%x)です\n", size, adrs);
}

static unsigned long
peek (address adrs, opesize size)
{
    unsigned long rc;

    switch (size) {
    case BYTESIZE:
	rc = PEEK_BYTE (adrs);
	break;
    case WORDSIZE:
	peek_warning (adrs, 'w');
	rc = PEEK_WORD (adrs);
	break;
    case LONGSIZE:
	peek_warning (adrs, 'l');
	rc = PEEK_LONG (adrs);
	break;
    default:
	rc = 0;
	break;
    }
    /*
	printf ("peek.%d(%x) = %x\n", size, (unsigned int)adrs, rc);
    */
    return rc;
}



static unsigned long
extend (unsigned long arg, opesize size)
{
    if (size == WORDSIZE)
	return (unsigned short)(signed char)arg;
    else /* if (size == LONGSIZE) */
	return (unsigned long)(signed short)arg;
}


static void
breakjob (unsigned long value)
{
    if (Eval_Count == 0)			/* easy trick ? */
	Eval_Count = 1;
    Eval_ID = BREAKID;
    Eval_Break = value;
}


#ifdef SELF
int
main (void)
{
    char buf[256];

    yydebug = 0;
    while (fgets (buf, sizeof buf, stdin)) {
	char* p = strchr (buf, '\n');
	if (p)
	    *p = '\n';
	Lexptr = buf;
	yyparse ();
    }
    return 0;
}
#endif /* SELF */


/* EOF */
