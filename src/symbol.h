/* $Id: symbol.h,v 1.1 1996/10/24 04:27:50 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	シンボルネーム管理モジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	SYMBOL_H
#define	SYMBOL_H


typedef struct _symlist {
    struct _symlist* next;
    UWORD  type;
    char*  sym;
} symlist;

typedef struct {
    address adrs;
    symlist first;
} symbol;


#ifndef	OSKDIS
#define	XDEF_COMMON	0x0003
#define	XDEF_ABS	0x0200
#define	XDEF_TEXT	0x0201
#define	XDEF_DATA	0x0202
#define	XDEF_BSS	0x0203
#define	XDEF_STACK	0x0204
#endif


extern void	init_symtable (void);
extern void	free_symbuf (void);
extern boolean	is_exist_symbol (void);
#ifndef	OSKDIS
extern int	output_symbol_table (const char*, const char*, const char*);
extern void	make_symtable (void);
extern symlist*	symbol_search2 (address adrs, int type);
#endif
extern void	add_symbol (address, int, char*);
extern symbol*	symbol_search (address);

#if 0
extern void	del_symbol (address);
#endif


extern short	Output_Symbol;


#endif	/* SYMBOL_H */

/* EOF */
