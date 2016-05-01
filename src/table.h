/* $Id: table.h,v 1.1 1996/10/24 04:27:50 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	テーブル処理モジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	TABLE_H
#define	TABLE_H

#include "etc.h"	/* peekl */


/* p には Ofst を足していない値を渡す */
#define PEEK_BYTE(p) ((UBYTE)(BeginBSS <= (p) ? 0 : *((p) + Ofst)))
#define PEEK_WORD(p) ((UWORD)(BeginBSS <= (p) ? 0 : (((p) + Ofst)[0] << 8) \
						   + ((p) + Ofst)[1]))
#if defined (__mc68020__) || defined (__i386__)
#define PEEK_LONG(p) ((ULONG)(BeginBSS <= (p) ? 0 : peekl ((p) + Ofst)))
#else
#define PEEK_LONG(p) ((ULONG)(BeginBSS <= (p) ? 0 : ((int)(p) & 1) ? \
		(peekl ((p) + Ofst - 1) << 8) + ((p) + Ofst)[3] : peekl ((p) + Ofst)))
#endif


/*
  テーブルの構造
  １アドレスに１つの table がある
  table.formulaptr[ 0 ... table.lines - 1 ] がテーブルのメンバを保持している
*/

typedef struct {
#if 0
    int     count;	/* now, count is evaluated each time */
#endif
    opesize id;
    boolean hidden;
    int     line;
    char*   expr;
} formula;

typedef struct {
    address tabletop;
    int     loop;
    int     lines;
    formula *formulaptr;	/* formula の配列へのポインタ */
} table;

extern void	read_tablefile (char*);
extern table*	search_table (address);

typedef enum {
    PARSE_ANALYZING ,
    PARSE_GENERATING ,
} parse_mode;


extern parse_mode ParseMode;
extern char*	Lexptr;
extern int	Eval_Value;
extern address	Eval_TableTop;
extern address	Eval_PC;
extern int	Eval_Bytes;
extern opesize	Eval_ID;
extern int	Eval_Count;
extern int	Eval_Break;
extern opesize	Eval_SizeID;
extern char	Eval_ResultString[256];


#endif	/* TABLE_H */

/* EOF */
