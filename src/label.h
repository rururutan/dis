/* $Id: label.h,v 1.1 1996/10/24 04:27:48 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ラベル管理モジュールヘッダ
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#ifndef	LABEL_H
#define	LABEL_H


typedef enum {
    PROLABEL =	0,			/* プログラム */
    DATLABEL =	0x010000,		/* データ */
    FORCE    =	0x020000,		/* 強制フラグ */
    HIDDEN   =	0x040000,		/* 1 ならそのラベルはソース中に現れない */
    TABLE    =	0x080000,		/* 1 でテーブル開始 */
    ENDTABLE =	0x100000,		/* 1 でテーブル終了 */
    SYMLABEL =	0x200000		/* 1 でシンボル情報あり */
#ifdef	OSKDIS
    CODEPTR  =	0x400000,		/* 1 でコードポインタ */
    DATAPTR  =	0x800000,		/* 1 でデータポインタ */
#endif	/* OSKDIS */
} lblmode;


struct _avl_node;
typedef struct {
    address		label;		/* アドレス */
    struct _avl_node	*avl;		/* AVL-tree-library side node */
    lblmode		mode;		/* 属性 */
    short		shift;		/* ずれ */
    unsigned short	count;		/* 登録回数 */
} lblbuf;


/*

  lblmode	下位８ビット	オペレーションサイズ(データラベルの時のみ意味を持つ)
		第16ビット	0...PROLABEL	1...DATLABEL
		第17ビット	0...普通	1...強制
  shift		ラベルアドレスとのずれ.通常０.
		命令のオペランドにラベルがあったりすると...

*/


extern void	init_labelbuf (void);
extern void	free_labelbuf (void);
extern boolean	regist_label (address adrs, lblmode mode);
extern void	unregist_label (address adrs);
extern lblbuf*	search_label (address adrs);
extern lblbuf*	next(address adrs);
extern lblbuf*	Next (lblbuf*);
extern lblbuf*	next_prolabel (address adrs);
extern lblbuf*	next_datlabel (address adrs);
extern int	get_Labelnum (void);

#define isPROLABEL(a)	(!isDATLABEL(a))
#define isDATLABEL(a)	((a) & DATLABEL)
#define isHIDDEN(a)	((a) & HIDDEN)
#define isTABLE(a)	((a) & TABLE)
#define isENDTABLE(a)	((a) & ENDTABLE)


#endif	/* LABEL_H */

/* EOF */
