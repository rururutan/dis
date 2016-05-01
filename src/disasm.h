/* $Id: disasm.h,v 1.1 1996/10/24 04:27:42 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	逆アセンブルモジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe, 1994 R.ShimiZu
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	DISASM_H
#define	DISASM_H


#include "estruct.h"


#define MAX_MACRO_LEN	16
#define SXCALL_MAX	0x1000	/* SX Window use 0xa000 - 0xafff */


/* MPU_types */
#define M000	(1<<0)		/* 68000 */
#define M010	(1<<1)		/* 68010 */
#define M020	(1<<2)		/* 68020 */
#define M030	(1<<3)		/* 68030 */
#define M040	(1<<4)		/* 68040 */
#define M060	(1<<6)		/* 68060 */
#define MISP	(1<<7)		/* 060ISP(software emulation) */

typedef unsigned char mputypes;

/* FPCP_type */
#define F881	(1<<0)		/* 68881 */
#define F882	(1<<1)		/* 68882 */
#define F88x	(F881|F882)
#define F040	(1<<2)		/* 68040 */
#define F4SP	(1<<3)		/* 040FPSP(software emulation) */
#define F060	(1<<4)		/* 68060 */
#define F6SP	(1<<5)		/* 060FPSP(software emulation) */

/* MMU_type */
#define MMU851	(1<<0)		/* 68020 + 68851 */
#define MMU030	(1<<1)		/* 68030 internal MMU */
#define MMU040	(1<<2)		/* 68040 internal MMU */
#define MMU060	(1<<3)		/* 68060 internal MMU */


typedef enum {
    OTHER ,		/* 普通の命令 */
    JMPOP ,		/* 分岐命令 */
    JSROP ,		/* サブルーチンコール命令 */
    RTSOP ,		/* リターン命令 */
    BCCOP ,		/* 条件分岐命令 */
    UNDEF = 15 ,	/* 未定義 */
} opetype;


typedef enum {
    DregD ,		/* データレジスタ直接 */
    AregD ,		/* アドレスレジスタ直接 */
    AregID ,		/* アドレスレジスタ間接 */
    AregIDPI ,		/* ポストインクリメントアドレスレジスタ間接 */
    AregIDPD ,		/* プリデクリメントアドレスレジスタ間接 */
    AregDISP ,		/* ディスプレースメント付アドレスレジスタ間接 */
    AregIDX ,		/* インデックス付アドレスレジスタ間接 */
    AbShort = 8 ,	/* 絶対ショートアドレス */
    AbLong ,		/* 絶対ロングアドレス */
    PCDISP ,		/* ディスプレースメント付プログラムカウンタ相対 */
    PCIDX ,		/* インデックス付プログラムカウンタ相対 */
    IMMED ,		/* イミディエイトデータ */
    SRCCR = 16 ,	/* CCR / SR 形式 */

    AregIDXB,		/* インデックス&ベースディスプレースメント付きアドレスレジスタ間接 */
    AregPOSTIDX,	/* ポストインデックス付きメモリ間接 */
    AregPREIDX,		/* プリインデックス付きメモリ間接 */
    PCIDXB,		/* インデックス&ベースディスプレースメント付きプログラムカウンタ間接 */
    PCPOSTIDX,		/* ポストインデックス付きPCメモリ間接 */
    PCPREIDX,		/* プリインデックス付きPCメモリ間接 */

    CtrlReg,		/* 制御レジスタ */

    RegPairD,		/* レジスタペア(直接) dx:dy */
    RegPairID,		/* レジスタペア(間接) (rx):(ry) */
    BitField,		/* ビットフィールドの {offset:width} */

    MMUreg,		/* MMUレジスタ */

    FPregD,		/* FPn */
    FPCRSR,		/* FPSR,FPCR,FPIAR */
    FPPairD,		/* レジスタペア(直接) FPx:FPy */
    KFactor,		/* K-Factor {offset:width} */

} adrmode;


typedef struct {
    char	    operand[ 64 ];	/* オペランド文字列 */
    adrmode	    ea;			/* 実効アドレスモード */
    address	    opval;		/* オペランドの値 */
    address	    opval2;		/* オペランドの値(od用) */
    address	    eaadrs;		/* オペランドの存在アドレス */
    address	    eaadrs2;		/* オペランドの存在アドレス(od用) */
    unsigned char   labelchange1;	/* ラベル化可能 -1なら()なし(bsr用) */
    unsigned char   labelchange2;	/* ラベル化可能 */
    unsigned char   exbd;		/* bd のサイズ(0,2,4)  0ならサプレス */
    unsigned char   exod;		/* od のサイズ(0,2,4)  0ならサプレス */
} operand;


typedef struct {
    char    opecode[ 32 ];  /* 命令 */
    opesize size;	    /* サイズ ( lea , pea は long ) ( 0 = .b .w .l .s nothing ) */
    opesize size2;	    /* サイズ ( lea, pea, moveq, bset, ... は UNKNOWN ) */
    opesize default_size;   /* その命令のデフォルトのサイズ */
    int     bytes;	    /* 命令のバイト数 */
    opetype flag;	    /* 命令の種類 ( 0 = other jmp jsr rts bcc undef ) */
    mputypes mputypes;	    /* この命令を実行可能なMPUの種類(M000|M010|...) */
    char    fpuid;	    /* 浮動小数点命令のコプロセッサID(0-7,-1なら通常命令) */
    char    opflags;	    /* FLAGS_xxx */
    char    reserved;	    /* 予約 */
    address jmp;	    /* ジャンプ先アドレス ( 分岐命令なら ) */
    adrmode jmpea;	    /* 実効アドレスモード ( 分岐命令なら ) */
    operand op1;
    operand op2;
    operand op3;
    operand op4;
} disasm;

#define FLAG_CANNOT_UPPER	0x01
#define FLAG_NEED_COMMENT	0x02
#define FLAG_NEED_NULSTR	0x04

#ifndef OSKDIS
extern char**	OSlabel;
extern char**	FElabel;
extern char**	SXlabel;
extern char	OSCallName[MAX_MACRO_LEN];
extern char	FECallName[MAX_MACRO_LEN];
extern char	SXCallName[MAX_MACRO_LEN];
#endif
extern char	FPUID_table[16];
extern boolean	Disasm_Exact;
extern boolean	Disasm_String;
extern boolean	Disasm_UnusedTrapUndefined;
extern boolean	Disasm_AddressErrorUndefined;
extern boolean	Disasm_Dbra;
extern boolean	Disasm_MnemonicAbbreviation;
extern boolean	Disasm_SX_Window;
extern boolean	Disasm_CPU32;
extern boolean	Disasm_ColdFire;
extern mputypes	MPU_types;
extern short	MMU_type;
extern short	FPCP_type;
extern int	UndefRegLevel;
extern address	PCEND;

extern int	dis (address, disasm*, address*);


#endif	/* DISASM_H */

/* EOF */
