/* $Id: offset.h,v 1.1 1996/10/24 04:27:48 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	再配置テーブル管理ヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	OFFSET_H
#define	OFFSET_H


extern void	make_relocate_table (void);
extern void	free_relocate_buffer (void);
extern boolean	depend_address (address);
extern address	nearadrs (address);

#define INPROG(opval, eaadrs) \
    (depend_address (eaadrs) || \
    (Absolute == ABSOLUTE_ZFILE && Head.base - 0x100 <= opval) || \
    (Absolute == ABSOLUTE_ZOPT && Head.base <= opval && opval <= Last))


#endif	/* OFFSET_H */

/* EOF */
