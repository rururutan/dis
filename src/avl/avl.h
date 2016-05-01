/*

  Generic AVL-tree library header file.

  Copyright (C) 1991 K.Abe

  E-Mail	k-abe@ics.osaka-u.ac.jp
  Nifty Serve	PDC02373


  AVL_delete() bugfix patch:
    Copyleft 1997 Tachibana Eriko.


  This file is included
    1) to use avl-library
    2) to make avl-library	( #define AVL_LIBRARY )
    3) to include avl-library	( #define AVL_INCLUDE )
*/

#ifndef __INCLUDE_AVL_H__
#define __INCLUDE_AVL_H__


#if defined( PROFILE ) && defined( __HUMAN68K__ )
  #define avl_private extern
#else
  #define avl_private static
#endif


#ifdef AVL_LIBRARY
  /* to make avl-library */
  #define AVL_USERDATA		void
  #ifndef AVL_NOMACRO
    #define AVL_NOMACRO
  #endif /* AVL_NOMACRO */
  #define avl_public extern
#elif defined( AVL_INCLUDE )
  /* include avl-library */
  #ifndef AVL_USERDATA
    #error You must define AVL_USERDATA before include this file.
  #endif /* AVL_USERDATA */
  #define avl_public avl_private
#else
  /* use avl-library */
  #ifndef AVL_USERDATA
    #define AVL_USERDATA	void
  #endif /* AVL_USERDATA */
  #define avl_public extern
#endif


typedef struct _avl_node {
    /* AVL-tree stuff */
    struct _avl_node	*left;
    struct _avl_node	*right;
    struct _avl_node	*parent;
    unsigned short	left_depth;
    unsigned short	right_depth;
    /* linked-list stuff */
    struct _avl_node	*next;
    struct _avl_node	*previous;
    /* user data */
    AVL_USERDATA	*data;
} avl_node;


typedef struct _avl_root_node {
    struct _avl_node	*avl_tree;
    struct _avl_node	*min;
    struct _avl_node	*max;
    int			data_number;
    int			(*compare_function)( AVL_USERDATA* , AVL_USERDATA* );
    void		(*free_function)( AVL_USERDATA* );
    void		(*print_function)( AVL_USERDATA* );
} avl_root_node;


#ifdef AVL_NOMACRO
avl_public AVL_USERDATA	*AVL_get_data(avl_node *node_ptr);
avl_public int		AVL_data_number(avl_root_node *root);
avl_public avl_node	*AVL_get_min(avl_root_node *root);
avl_public avl_node	*AVL_get_max(avl_root_node *root);
avl_public avl_node	*AVL_next(avl_node *node_ptr);
avl_public avl_node	*AVL_previous(avl_node *node_ptr);
#endif /* AVL_NOMACRO */

avl_public avl_root_node	*AVL_create_tree( int (*compare_function)
					 ( AVL_USERDATA* , AVL_USERDATA* ) ,
					  void (*free_function)( AVL_USERDATA* ) ,
					  void (*print_function)( AVL_USERDATA* ) );
avl_public void		AVL_destroy_tree(avl_root_node *root);
avl_public avl_node	*AVL_insert(avl_root_node *root, AVL_USERDATA *data);
avl_public void		AVL_delete(avl_root_node *root, avl_node *delete_node);
avl_public avl_node	*AVL_search(avl_root_node *root, AVL_USERDATA *data);
avl_public avl_node	*AVL_search_next(avl_root_node *root, AVL_USERDATA *data);
avl_public avl_node	*AVL_search_previous(avl_root_node *root, AVL_USERDATA *data);
avl_public void		AVL_print_tree(avl_root_node *root);
avl_public void		AVL_check_tree(avl_root_node *root);


#ifndef AVL_NOMACRO
#define	AVL_get_data(node_ptr)	((node_ptr)->data)
#define AVL_get_min(root)	((root)->min)
#define AVL_get_max(root)	((root)->max)
#define	AVL_data_number(root)	((root)->data_number)
#define	AVL_next(node_ptr)	((node_ptr)->next)
#define	AVL_previous(node_ptr)	((node_ptr)->previous)
#endif


/* AVL_get_data_safely is function rather than macro
   because macro will evaluate its argument twice.  */
static
#ifdef __GNUC__
inline
#endif
AVL_USERDATA	*AVL_get_data_safely( node_ptr )
avl_node	*node_ptr;
{
    return( node_ptr != NULL ? node_ptr->data : NULL );
}

#undef avl_public
#undef avl_private

#endif /* __INCLUDE_AVL_H__ */

/* EOF */
