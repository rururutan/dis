/* $Id: include.h,v 1.1 1996/10/24 04:27:46 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	file include header
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	INCLUDE_H
#define	INCLUDE_H

extern void load_OS_sym (void);

#ifndef	OSKDIS
extern const char*  Doscall_mac_path;
extern const char* Iocscall_mac_path;
extern const char*   Fefunc_mac_path;
extern const char*   Sxcall_mac_path;

extern const char*  doscall_mac;
extern const char* iocscall_mac;
extern const char*   fefunc_mac;

#endif

#endif	/* INCLUDE_H */

/* EOF */
