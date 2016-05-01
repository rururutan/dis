/* $Id: output.c,v 1.1 1996/11/07 08:03:56 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	出力ルーチン下請け
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __LIBC__
  #define __DOS_INLINE__
  #include <sys/dos.h>		/* _dos_getfcb */
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif


#include "disasm.h"
#include "estruct.h"
#include "etc.h"	/* err */
#include "generate.h"	/* Atab */
#include "global.h"
#include "hex.h"	/* strend */
#include "output.h"


USEOPTION	option_a, option_S;

extern char	CommentChar;

int	Output_AddressCommentLine;
int	Output_SplitByte;

#ifdef STREAM
static FILE*	Output_fp;
#else
static int	Output_handle;
static char*	OutputLargeBuffer;
static char*	OutputLargeBufferPtr;
#endif
static char	OutputBuffer[256];
static char*	OutputBufferEnd = OutputBuffer;
static boolean	SplitMode;

#define OUTPUT_BUFFER_SIZE	(64*1024)


/* private 関数プロトタイプ */
#ifndef STREAM
private void	flush_buffer (void);
#endif


/* Inline Functions */

static INLINE char*
strcpy2 (char* dst, char* src)
{
    while ((*dst++ = *src++) != 0)
	;
    return --dst;
}


/*

  初期化

*/
extern void
init_output (void)
{
#ifndef STREAM
    if ((OutputLargeBuffer = Malloc (OUTPUT_BUFFER_SIZE)) == NULL)
	err ("出力バッファ用メモリを確保出来ません\n");
    OutputLargeBufferPtr = OutputLargeBuffer;
#endif
}


/*

  出力ファイルをオープン

  char* filename == NULLなら前回指定されたファイル名を引き続き使用する.
  通常は
	output_file_open ("foo", 0);	 // foo.000
	output_file_open (NULL, 1);	 // foo.001
		...
	output_file_open (NULL, -1);	 // foo.dat
	output_file_open (NULL, -2);	 // foo.bss
  として呼び出す.

*/

extern void
output_file_open (char* filename, int file_block_num)
{
    static char* basename;

    if (filename)
	basename = filename;

    if (basename == NULL)
	err ("dis: internal error!\n");

    if (strcmp ("-", basename) == 0) {
#ifdef STREAM
	Output_fp = stdout;
#else
	Output_handle = STDOUT_FILENO;
#endif
    } else {
	char* buf = Malloc (strlen (basename) + 4 + 1);
	strcpy (buf, basename);

	if (option_S) {
	    switch (file_block_num) {
	    case 0:
		SplitMode = TRUE;
		/* fall through */
	    default:
		sprintf (strend (buf), ".%03x", file_block_num);
		break;
	    case -1:
		strcat (buf, ".dat");
		SplitMode = FALSE;
		break;
	    case -2:
		strcat (buf, ".bss");
		SplitMode = FALSE;
		break;
	    }
	}

#ifdef	STREAM
	if ((Output_fp = fopen (buf, "w")) == NULL)
#else
#ifdef	OSK
	if ((Output_handle = creat (buf, S_IREAD|S_IWRITE)) < 0)
#else
	if ((Output_handle = open (buf,
		O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IRUSR | S_IWUSR)) < 0)
#endif	/* OSK */
	    err ("%s をオープン出来ません.\n", buf);
#endif

	Mfree (buf);
    }
}


extern void
output_file_close (void)
{
#ifdef STREAM
    fclose (Output_fp);
#else
    flush_buffer ();
    close (Output_handle);
#endif
}


/*

  書き込みエラー

*/
private void
diskfull (void)
{
#ifdef STREAM
    fclose (Output_fp);
#else
    close (Output_handle);
#endif
    err ("\nディスクが一杯です\n");
}


#ifndef STREAM
private void
output_through_buffer (char* str)
{
    
    OutputLargeBufferPtr = strcpy2 (OutputLargeBufferPtr, str);

    if (OutputLargeBufferPtr - OutputLargeBuffer > OUTPUT_BUFFER_SIZE - 1024)
	flush_buffer ();
}


/*

  ファイルへ出力

*/
extern void
outputf (char* fmt, ...)
{
#ifndef STREAM
    char tmp[256];
#endif
    va_list ap;
    va_start (ap, fmt);

#ifdef STREAM
    vfprintf (Output_fp, fmt, ap);
    va_end (ap);
    if (ferror (Output_fp))
	diskfull ();
#else
    vsprintf (tmp, fmt, ap);
    va_end (ap);
    output_through_buffer (tmp);
#endif
}


extern void
outputa (char* str)
{
    OutputBufferEnd = strcpy2 (OutputBufferEnd, str);
}


extern void
outputca (int ch)
{
    *OutputBufferEnd++ = ch;
    *OutputBufferEnd   = '\0';
}


extern void
outputfa (char* fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vsprintf (OutputBufferEnd, fmt, ap);
    OutputBufferEnd = strend (OutputBufferEnd);
    va_end (ap);
}


#if 0
extern void
outputax (ULONG n, int width)
{
    OutputBufferEnd = itox (OutputBufferEnd, n, width);
}
#endif

extern void
outputaxd (ULONG n, int width)
{
    OutputBufferEnd = itoxd (OutputBufferEnd, n, width);
}

extern void
outputax2_without_0supress (ULONG n)
{
    *OutputBufferEnd++ = '$';
    OutputBufferEnd = itox2_without_0supress (OutputBufferEnd, n);
}

extern void
outputax4_without_0supress (ULONG n)
{
    *OutputBufferEnd++ = '$';
    OutputBufferEnd = itox4_without_0supress (OutputBufferEnd, n);
}

extern void
outputax8_without_0supress (ULONG n)
{
    *OutputBufferEnd++ = '$';
    OutputBufferEnd = itox8_without_0supress (OutputBufferEnd, n);
}


extern void
newline (address lineadrs)
{

    if (option_S && SplitMode) {
	static int file_block_num = 0;

	if (Output_SplitByte <=
		((long)lineadrs - (long)BeginTEXT - Output_SplitByte * file_block_num)
		&& OutputBuffer[0] == '\n') {
	    output_file_close ();
	    output_file_open (NULL, ++file_block_num);
	}
    }

    if (option_a) {
	static int linecount = 1;

	if (linecount >= Output_AddressCommentLine) {
	    char* ptr;
	    int i, len = 0;

	    /* 桁数を数える */
	    for (ptr = OutputBuffer; *ptr; ptr++) {
		if (*ptr == '\t') {
		    len |= 7;
		    len++;
		}
		else if (*ptr == '\n')
		    len = 0;
		else
		    len++;
	    }

	    /* 規定位置までタブを出力する */
	    for (i = Atab - len / 8 - 1; i > 0; i--)
		*OutputBufferEnd++ = '\t';

	    *OutputBufferEnd++ = '\t';
	    *OutputBufferEnd++ = CommentChar;

	    OutputBufferEnd = itox6 (OutputBufferEnd, (ULONG)lineadrs);
	    linecount = 1;
	}
	else
	    linecount++;
    }

    *OutputBufferEnd++ = '\n';
    *OutputBufferEnd   = '\0';

#ifdef STREAM
    fputs (OutputBuffer, Output_fp);
    if (ferror (Output_fp))
	diskfull ();
#else
    output_through_buffer (OutputBuffer);
#endif
    OutputBufferEnd = OutputBuffer;
}


private void
flush_buffer (void)
{
    long length = OutputLargeBufferPtr - OutputLargeBuffer;

    if (write (Output_handle, OutputLargeBuffer, length) < length)
	diskfull ();

    OutputLargeBufferPtr = OutputLargeBuffer;
}
#endif /* STREAM */


/*
	ソースファイルの出力先と、標準エラー出力が同じなら真を返す.

	dis foo.x
		どちらも端末なので、真.

	dis foo.x >& list
		どちらも list なので、真

	dis foo.x foo.s
		foo.s と端末なので、偽.
*/

extern boolean
is_confuse_output (void)
{
#ifdef	STREAM
    int src_no = fileno (Output_fp);
#else
    int src_no = Output_handle;
#endif

    if (isatty (src_no) && isatty (STDERR_FILENO))
	return TRUE;

#ifdef __LIBC__
    if (_dos_getfcb (src_no) == _dos_getfcb (STDERR_FILENO))
	return TRUE;
#else
    {
	struct stat src_st, err_st;

	if (fstat (src_no, &src_st) == 0
	 && fstat (STDERR_FILENO, &err_st) == 0
	 && src_st.st_dev == err_st.st_dev
	 && src_st.st_ino == err_st.st_ino)
	    return TRUE;
    }
#endif

    return FALSE;
}


#if 0
/* following functions is no longer used. */

extern void
output (char* str)
{
    fputs (str, Output_fp);
    if (ferror (Output_fp))
	diskfull ();
}

extern void
outputc (int ch)
{
    fputc (ch, Output_fp);
}
#endif


/* EOF */
