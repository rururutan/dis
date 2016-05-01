/* $Id: output.h,v 1.1 1996/10/24 04:27:50 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	出力ルーチン下請けヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	OUTPUT_H
#define	OUTPUT_H


extern void init_output (void);
extern void output_file_open (char* , int);
extern void output_file_close (void);
extern void outputf (char*, ...);
extern void outputa (char*);
extern void outputca (int);
extern void outputfa (char*, ...);
extern void newline (address);
#if 0
extern void outputax (ULONG, int);
#endif
extern void outputaxd (ULONG, int);
extern void outputax2_without_0supress (ULONG);
extern void outputax4_without_0supress (ULONG);
extern void outputax8_without_0supress (ULONG);

extern boolean	is_confuse_output (void);

#if 0
extern void output (char*);
extern void outputc (int);
#endif

#define CR  "\n"

extern	int	Output_AddressCommentLine;
extern	int	Output_SplitByte;


#endif	/* OUTPUT_H */

/* EOF */
