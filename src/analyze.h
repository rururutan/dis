/* $Id: analyze.h,v 1.1 1996/10/24 04:27:42 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	自動解析モジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	ANALYZE_H
#define	ANALYZE_H

#include "estruct.h"		/* typedef address, enum boolean */
#include "label.h"		/* typedef lblmode */


typedef enum {
    ANALYZE_IGNOREFAULT,
    ANALYZE_NORMAL
} analyze_mode;

extern boolean	analyze (address, analyze_mode);
extern void	not_program (address, address);
extern boolean	ch_lblmod (address, lblmode);
extern void	relative_table (address);
extern void	relative_longtable (address);
extern void	z_table (address);

extern int	research_data (void);
extern void	analyze_data (void);
extern void	search_adrs_table (void);
extern void	search_operand_label (void);
extern int	search_string (int);

extern boolean	Reason_verbose;
extern boolean	Arg_after_call;

#endif	/* ANALYZE_H */

/* EOF */
