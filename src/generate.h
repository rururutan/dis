/* $Id: generate.h,v 1.1 1996/10/24 04:27:44 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ソースコードジェネレートモジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	GENERATE_H
#define	GENERATE_H


#include <time.h>	/* typedef time_t */
#include "estruct.h"	/* typedef LONG, ULONG */

typedef enum {
    SIZE_NORMAL,
    SIZE_OMIT,
    SIZE_NOT_OMIT
} size_mode;

extern	void	generate (char*, char*, time_t, int, char*[]);
extern	void	disasmlist (char*, char*, time_t);
extern	void	byteout_for_xoption (address, ULONG, char*);
extern	void	modify_operand (disasm*);
extern	char*	make_symbol (char*, address, LONG);
extern	void	make_proper_symbol (char*, address);

extern	int		Data_width;
extern	int		String_width;
extern	int		Compress_len;
extern	int		Adrsline;
extern	int		Xtab, Atab;
extern	int		SymbolColonNum;
extern	char		Label_first_char;
extern	short		sp_a7_flag;
extern	short		Old_syntax;
extern	size_mode	Generate_SizeMode;

extern	char		opsize[];

#ifdef	OSKDIS
extern	char*		OS9label [0x100];
extern	char*		MATHlabel[0x100];
extern	char*		CIOlabel [0x100];
#else
extern	char**		IOCSlabel;
extern	char		IOCSCallName[16];
extern	const char*	Header_filename;
#endif	/* OSKDIS */


#endif	/* GENERATE_H */

/* EOF */
