/* $Id: label.c,v 1.1 1996/11/07 08:03:44 ryo freeze $
 *
 *	ソースコードジェネレータ
 *	ラベル管理モジュール
 *	Copyright (C) 1989,1990 K.Abe
 *	All rights reserved.
 *	Copyright (C) 1997-2010 Tachibana
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "estruct.h"
#include "etc.h"
#include "global.h"
#include "label.h"


#define AVL_USERDATA	lblbuf
#define AVL_INCLUDE
#include "./avl/avl.h"

#define	AVL_COMPARE(label1, label2) \
    (signed)((unsigned)(label1)->label - (unsigned)(label2)->label)

#include "./avl/avl.c"


static avl_root_node* LabelRoot;
static lblbuf Nomore = {
    (address)-1, NULL, DATLABEL | UNKNOWN, 0, 1
};


private int
compare (lblbuf* label1, lblbuf* label2)
{
    return (unsigned long)label1->label - (unsigned long)label2->label;
}


private void
print (lblbuf* label)
{
    printf ("%x", (unsigned int)label->label);
}


/*

  ラベルバッファを初期化する

*/
extern void
init_labelbuf (void)
{
    LabelRoot = AVL_create_tree (compare, (void (*)(AVL_USERDATA*))free, print);
}


/*

  後始末

*/
extern void
free_labelbuf (void)
{
    return;
}


/*

  adrs を mode 型のエントリアドレスとして登録する
  既に登録されていたら（登録されていたアドレスの型を oldmode とする）
	modeがプログラム && oldmodeがプログラム	FALSE
	modeがプログラム && oldmodeがデータ		登録し直し TRUE
	modeがデータ     && oldmodeがプログラム	FALSE
	modeがデータ     && oldmodeがデータ
	mode == KNOWN && oldmode == UNKNOWN		登録し直し
	oldmode != mode					登録し直し
	mode == UNKNOWN && oldmode == KNOWN		なにもしないFALSE を返す
	返り値は adrs から解析する必要があるかを表す( プログラムの場合 )

*/

extern boolean
regist_label (address adrs, lblmode mode)
{
    lblbuf* lptr;
    lblbuf* ins_buf;

    if (adrs < BeginTEXT || adrs > Last)
	return FALSE;

    if (isPROLABEL (mode)) {
	if ((long)adrs & 1)
	    return FALSE;
	else if (adrs > Available_text_end) {
	    regist_label (adrs, DATLABEL | UNKNOWN);
	    return FALSE;
	}
    }

    if (Debug & BLABEL)
	printf ("Regist 0x%.6x as %#x\n", (unsigned int) adrs, mode);

    if ((lptr = search_label (adrs)) != NULL) {
	lptr->count++;

	if (mode & TABLE) {
	    lptr->mode = (lptr->mode & (TABLE | ENDTABLE | SYMLABEL)) | mode;
	    if (Debug & BLABEL)
		printf ("registed(table) %x\n", lptr->mode);
	    return FALSE;
	}
	else if (mode & ENDTABLE) {
	    lptr->mode |= ENDTABLE;
	    if (Debug & BLABEL)
		printf ("registed(endtable) %x\n", lptr->mode);
	    return FALSE;
	}

	if ((lptr->mode & FORCE) && !(isDATLABEL (lptr->mode) && isDATLABEL (mode))) {
	    if (Debug & BLABEL)
		printf ("but denied\n");
	    return FALSE;
	}
	if (mode & FORCE) {
	    lptr->mode = (lptr->mode & (TABLE | ENDTABLE | SYMLABEL)) | mode;
	    if (Debug & BLABEL)
		printf ("regist : %x(%d) was forced to %x\n",
				(unsigned int) adrs, lptr->count, lptr->mode);
	    return TRUE;
	}

	lptr->mode &= ~HIDDEN;

	if (isPROLABEL (mode)) {
	    if (isDATLABEL (lptr->mode)) {
		lptr->mode = (lptr->mode & (ENDTABLE | SYMLABEL)) | PROLABEL;
		if (Debug & BLABEL)
		    printf ("regist : %x(%d) was prog. %x\n",
				(unsigned int) adrs, lptr->count, lptr->mode);
	    }
	    return TRUE;
	}
	else {				/* if (isDATLABEL (mode)) */
	    if (isPROLABEL (lptr->mode)) {
		if (Debug & BLABEL)
		    printf ("but denied\n");
		return FALSE;
	    }
	    else {			/* if (isDATLABEL (lptr->mode)) */
		if (!((mode & 0xff) == UNKNOWN && (lptr->mode & 0xff) != UNKNOWN))
		    lptr->mode = (lptr->mode & (FORCE | TABLE | ENDTABLE | SYMLABEL))
				| mode;
		if (Debug & BLABEL)
		    printf ("changed surely(%x)\n", lptr->mode);
		return TRUE;
	    }
	}
	/* NOT REACHED */
    }

    ins_buf = Malloc (sizeof (lblbuf));
    ins_buf->label = adrs;
    ins_buf->mode = mode;
    ins_buf->shift = 0;
    ins_buf->count = 1;

    ins_buf->avl = AVL_insert (LabelRoot, ins_buf);
#ifdef AVL_DEBUG
    AVL_print_tree (LabelRoot);
    printf ("\n");
#endif
    return TRUE;
}


private avl_node*
search_label2 (address adrs)
{
    lblbuf search_data;

    search_data.label = adrs;
    return AVL_search (LabelRoot, &search_data);
}


/*

  ラベルの登録を取り消す

*/
extern void
unregist_label (address adrs)
{
    avl_node* del;

    if (Debug & BLABEL)
	printf ("* unregist_label %x\n", (unsigned int) adrs);

    if ((del = search_label2 (adrs)) == NULL) {
	if (Debug & BLABEL)
	    printf ("unregist_label : %x was not registed\n", (unsigned int) adrs);
    }
    else {
	lblbuf* ptr = (lblbuf*) AVL_get_data (del);

	if (--(ptr->count) == 0 && !(ptr->mode & SYMLABEL))
	    AVL_delete (LabelRoot, del);
    }
}


/*

  ラベルバッファからadrsを捜す
  あればバッファに対するポインタ
  なければ NULL を返す

*/
extern lblbuf*
search_label (address adrs)
{
    lblbuf search_data;

    search_data.label = adrs;
    return AVL_get_data_safely (AVL_search (LabelRoot, &search_data));
}


/*

  adrs の次のラベルバッファへのポインタを返す
  返り値->label が -1 ならバッファエンプティ
  adrs と等しいラベルがあればそれが返ってくるので注意！

*/
extern lblbuf*
next (address adrs)
{
    lblbuf search_data;
    avl_node* node;

    if (adrs == (address)-1)
	return &Nomore;

    search_data.label = adrs;

    if ((node = AVL_search_next (LabelRoot, &search_data)) == NULL)
	return &Nomore;
    else
	return AVL_get_data (node);
}


/*

  lptr の次のラベルバッファへのポインタを返す
  返り値->label が -1 ならバッファエンプティ
  adrs と等しいラベルがあればそれが返ってくるので注意！

*/
extern lblbuf*
Next (lblbuf* lptr)
{
    lblbuf* rc = AVL_get_data_safely (AVL_next (lptr->avl));

    return rc ? rc : &Nomore;
}


/*

  adrs から PROLABEL を探す
  adrs が PROLABEL ならそれが返ってくる

*/
extern lblbuf*
next_prolabel (address adrs)
{
    avl_node* node_ptr;
    lblbuf search_data;

#ifdef	DEBUG
    printf ("next_prolabel(%x)=", adrs);
#endif

    if (adrs == (address)-1)
	return &Nomore;

    search_data.label = adrs;
    node_ptr = AVL_search_next (LabelRoot, &search_data);
    while (node_ptr && !isPROLABEL (((lblbuf*) AVL_get_data (node_ptr))->mode))
	node_ptr = AVL_next (node_ptr);

    if (node_ptr == NULL)
	return &Nomore;

#ifdef	DEBUG
    printf ("%x\n", AVL_get_data (node_ptr));
#endif
    return (lblbuf*) AVL_get_data (node_ptr);
}


/*

  adrs から DATLABEL を探す
  adrs が DATLABEL ならそれが返ってくる

*/
extern lblbuf*
next_datlabel (address adrs)
{
    avl_node* node_ptr;
    lblbuf  search_data;
    lblbuf* lblptr = &Nomore;

#ifdef	DEBUG
    printf ("next_datlabel(%x)=", adrs);
#endif

    if (adrs == (address)-1)
	return &Nomore;

    search_data.label = adrs;
    node_ptr = AVL_search_next (LabelRoot, &search_data);
    while (node_ptr && (!isDATLABEL ((lblptr = AVL_get_data (node_ptr))->mode)
			|| lblptr->shift))
	node_ptr = AVL_next (node_ptr);

    if (node_ptr == NULL)
	return &Nomore;

#ifdef	DEBUG
    printf ("%x(%x), %d\n", lblptr->label, lblptr->mode, lblptr->count);
#endif
    return lblptr;
}


extern int
get_Labelnum (void)
{
    return AVL_data_number (LabelRoot);
}

/* EOF */
