/*

  Generic AVL-tree library

  Copyright (C) 1991 K.Abe

  E-Mail	k-abe@ics.osaka-u.ac.jp
  Nifty Serve	PDC02373


  AVL_delete() bugfix patch:
    Copyleft 1997 Tachibana Eriko.

*/

#include <stdio.h>
#include <stdlib.h>

#ifdef AVL_LIBRARY_ONCE
  #error You cannot include this file twice.
#endif
#define AVL_LIBRARY_ONCE

#include "avl.h"

#ifndef __GNUC__
  #define inline
#endif /* __GNUC__ */


#if defined( PROFILE ) && defined( __HUMAN68K__ )
  #define avl_private extern
#else
  #define avl_private static
#endif


/* avl_private functions prototypes */
avl_private inline avl_node	*avl_create_node(void);
avl_private inline int		avl_depth(avl_node *node_ptr);
avl_private inline void		avl_calc_node_depth(avl_node *node_ptr);
avl_private void		avl_adjust_depth(avl_root_node *root, avl_node *node_ptr);
avl_private avl_node		*avl_balance(avl_root_node *root, avl_node *node_ptr);
avl_private inline void		avl_adjust_parent(avl_root_node *root, avl_node *node_ptr,
						  avl_node *rotate_node);
avl_private void	avl_print_tree(avl_root_node *root, avl_node *node_ptr, int level);
avl_private int		avl_check_tree( avl_root_node* root, avl_node *node_ptr );


#ifdef AVL_LIBRARY
  /* if compiled as library. */
  #define AVL_COMPARE(data1,data2)	(*(root->compare_function))(data1,data2)
  #ifndef AVL_NOMACRO
    #define AVL_NOMACRO
  #endif /* AVL_NOMACRO */
  #define avl_public extern
#else
  /* if included */
  #ifndef AVL_COMPARE
    #error You must define AVL_COMPARE before include this file.
  #endif /* AVL_COMPARE */
  #define avl_public avl_private
#endif /* AVL_LIBRARY */

#define	AVL_BALANCE(node_ptr)	((node_ptr)->right_depth - (node_ptr)->left_depth)


#if 0
static char avl_version[] = "Generic AVL-tree Library v0.1 (Copyright (C) 1991 K.Abe)";
#endif



/*

  節点の回転の下請け処理

  node_ptrを指している(node_ptrの親節点)のnode_ptrへのポインタ(left or right)が
  rotate_node を指すようにする

       (Parent)               (Parent)<-----------\
     ↓↑    ↓↑       ->  ↓↑    ↓             \
     (node)  (node_ptr)     (node)  (rotate_node) (node_ptr)

*/
avl_private inline void	avl_adjust_parent( root , node_ptr , rotate_node )
avl_root_node	*root;
avl_node	*node_ptr , *rotate_node;
{
    if( node_ptr->parent == NULL )
	root->avl_tree = rotate_node;
    else if( node_ptr->parent->left == node_ptr )
	node_ptr->parent->left = rotate_node;
    else
	node_ptr->parent->right = rotate_node;
}


/*

  ある節点を根とした場合の木の高さを返す

*/
avl_private inline int	avl_depth( node_ptr )
avl_node	*node_ptr;
{
    if( node_ptr == NULL )
	return -1;
    return ( node_ptr->left_depth > node_ptr->right_depth ?
	     node_ptr->left_depth : node_ptr->right_depth );
}


/*

  節点の左右の部分木の深さを代入する

*/
avl_private inline void	avl_calc_node_depth( node_ptr )
avl_node	*node_ptr;
{
    if( node_ptr != NULL ) {
	node_ptr->left_depth = avl_depth( node_ptr->left ) + 1;
	node_ptr->right_depth = avl_depth( node_ptr->right ) + 1;
    }
}


/*

  ある節点から親に向かって、左右の部分木の高さを計算し直す
  バランスがくずれていたらバランス化する

*/
avl_private void	avl_adjust_depth( root , node_ptr )
avl_root_node	*root;
avl_node	*node_ptr;
{
    avl_node	*rotate_node;

    for( ; node_ptr != NULL ; node_ptr = node_ptr->parent ) {
	avl_calc_node_depth( node_ptr );

	if( AVL_BALANCE( node_ptr ) == 2 || AVL_BALANCE( node_ptr ) == -2 ) {
	    if( ( rotate_node = avl_balance( root , node_ptr ) ) != NULL ) {
		avl_calc_node_depth( rotate_node->left );
		avl_calc_node_depth( rotate_node->right );
		avl_calc_node_depth( rotate_node );
	    }
	}
    }
}


/*

  バランスがくずれた節点をバランス化する

*/
avl_private avl_node	*avl_balance( root , node_ptr )
avl_root_node	*root;
avl_node	*node_ptr;
{
    avl_node	*swap;
    avl_node	*left_node , *right_node;

    if( node_ptr->left_depth > node_ptr->right_depth ) {
	if( AVL_BALANCE( node_ptr->left ) * AVL_BALANCE( node_ptr ) > 0 ) {
	    avl_adjust_parent( root , node_ptr , node_ptr->left );
	    node_ptr->left->parent = node_ptr->parent;
	    swap = node_ptr->left->right;
	    if( swap ) swap->parent = node_ptr;
	    node_ptr->left->right = node_ptr;
	    node_ptr->parent = node_ptr->left;
	    node_ptr->left = swap;
	    return node_ptr->parent;
	} else /* if ( AVL_BALANCE( node_ptr->left ) * AVL_BALANCE( node_ptr ) < 0 ) */ {
	    left_node = node_ptr->left;
	    left_node->right->parent = node_ptr;
	    swap = left_node->right->left;
	    if( swap ) swap->parent = left_node;
	    left_node->right->left = left_node;
	    left_node->parent = left_node->right;
	    left_node->right = swap;
	    node_ptr->left = left_node->parent;

	    avl_adjust_parent( root , node_ptr , node_ptr->left );
	    swap = node_ptr->left->right;
	    if( swap ) swap->parent = node_ptr;
	    node_ptr->left->right = node_ptr;
	    node_ptr->left->parent = node_ptr->parent;
	    node_ptr->parent = node_ptr->left;
	    node_ptr->left = swap;
	    return node_ptr->parent;
	}
    } else if( node_ptr->left_depth < node_ptr->right_depth ) {
	if( AVL_BALANCE( node_ptr->right ) * AVL_BALANCE( node_ptr ) > 0 ) {
	    avl_adjust_parent( root , node_ptr , node_ptr->right );
	    node_ptr->right->parent = node_ptr->parent;
	    swap = node_ptr->right->left;
	    if( swap ) swap->parent = node_ptr;
	    node_ptr->right->left = node_ptr;
	    node_ptr->parent = node_ptr->right;
	    node_ptr->right = swap;
	    return node_ptr->parent;
	} else /* if( AVL_BALANCE( node_ptr->right ) * AVL_BALANCE( node_ptr ) < 0 ) */ {
	    right_node = node_ptr->right;
	    right_node->left->parent = node_ptr;
	    swap = right_node->left->right;
	    if( swap ) swap->parent = right_node;
	    right_node->left->right = right_node;
	    right_node->parent = right_node->left;
	    right_node->left = swap;
	    node_ptr->right = right_node->parent;

	    avl_adjust_parent( root , node_ptr , node_ptr->right );
	    swap = node_ptr->right->left;
	    if( swap ) swap->parent = node_ptr;
	    node_ptr->right->left = node_ptr;
	    node_ptr->right->parent = node_ptr->parent;
	    node_ptr->parent = node_ptr->right;
	    node_ptr->right = swap;
	    return node_ptr->parent;
	}
    }
    /* NOT REACHED */
    return NULL;
}


/*

  ライブラリ側の節点を作る

*/
avl_private inline avl_node	*avl_create_node()
{
    avl_node	*node_ptr;
    if( ( node_ptr = malloc( sizeof( avl_node ) ) ) == NULL ) {
	fputs( "create_node: malloc failed.\n" , stderr );
	exit( 1 );
    }
    node_ptr->left = node_ptr->right = node_ptr->parent = NULL;
    node_ptr->left_depth = node_ptr->right_depth = 0;
    node_ptr->next = node_ptr->previous = NULL;
    node_ptr->data = NULL;
    return node_ptr;
}


#ifdef AVL_NOMACRO
/*

  AVL-tree のノードからユーザーのデータを得る

*/
avl_public AVL_USERDATA	*AVL_get_data( node_ptr )
avl_node	*node_ptr;
{
    return node_ptr->data;
}


/*

  AVL-tree 中のノードの数を返す

*/
avl_public int	AVL_data_number( root )
avl_root_node	*root;
{
    return root->data_number;
}


/*

  最小のノードを返す

*/
avl_public avl_node	*AVL_get_min( root )
avl_root_node	*root;
{
    return root->min;
}


/*

  最大のノードを返す

*/
avl_public avl_node	*AVL_get_max( root )
avl_root_node	*root;
{
    return root->max;
}


/*

  次の要素を返す

*/
avl_public avl_node	*AVL_next( node_ptr )
avl_node	*node_ptr;
{
    return node_ptr->next;
}


/*

  前の要素を返す

*/
avl_public avl_node	*AVL_previous( node_ptr )
avl_node	*node_ptr;
{
    return node_ptr->previous;
}
#endif /* AVL_NOMACRO */



/*

  AVL-tree をつくる

*/
avl_public avl_root_node*	AVL_create_tree( compare_func , free_func , print_func )
int	(*compare_func)( AVL_USERDATA* , AVL_USERDATA* );
void	(*free_func)( AVL_USERDATA* );
void	(*print_func)( AVL_USERDATA* );
{
    avl_root_node	*root;
    if( ( root = malloc( sizeof( avl_root_node ) ) ) == NULL ) {
	fputs( "AVL_create_tree: malloc failed.\n" , stderr );
	exit( 1 );
    }
#ifdef AVL_LIBRARY
    if( compare_func == NULL ) {
	fprintf( stderr , "AVL_create_tree: compare function must not be NULL.\n" );
	exit( 1 );
    }
#endif /* AVL_LIBRARY */

    root->avl_tree = root->min = root->max = NULL;
    root->data_number = 0;
    root->compare_function = compare_func;
    root->free_function = free_func;
    root->print_function = print_func;
    return root;
}


/*

  AVL-tree を壊す

*/
avl_public void	AVL_destroy_tree( root )
avl_root_node	*root;
{
    avl_node	*node_ptr;
    avl_node	*next;

    for( node_ptr = root->avl_tree ; node_ptr != NULL ; node_ptr = next ) {
	next = node_ptr->next;
	(*( root->free_function ))( node_ptr->data );
	free( node_ptr );
    }
    free( root );
}


/*

  AVL-tree に挿入する

*/
avl_public avl_node	*AVL_insert( root , data )
avl_root_node	*root;
AVL_USERDATA	*data;
{
    avl_node	*node_ptr;
    avl_node	*old_ptr;
    avl_node	*new_node;
    avl_node	*last_right_node = NULL;
    int		comp = 0;

    /* do search */
    old_ptr = NULL;
    node_ptr = root->avl_tree;
    while( node_ptr != NULL ) {
	old_ptr = node_ptr;
	comp = AVL_COMPARE( data , node_ptr->data );
	if( comp < 0 )
	    node_ptr = node_ptr->left;
	else if( comp > 0 ) {
	    last_right_node = node_ptr;
	    node_ptr = node_ptr->right;
	} else {
	    /* Already exists. return NULL to caller. */
	    return NULL;
	}
    }
    /* do insert */
    root->data_number++;
    if( old_ptr == NULL ) {
	root->avl_tree = new_node = avl_create_node();
	new_node->data = data;
	root->min = root->max = new_node;
    } else {
	if( comp > 0 ) {
	    old_ptr->right = new_node = avl_create_node();	/* AVL tree */
	    old_ptr->right_depth = 1;
	    new_node->next = old_ptr->next;			/* linked-list */
	    new_node->previous = old_ptr;
	    old_ptr->next = new_node;
	    if( new_node->next )
		new_node->next->previous = new_node;
	    else
		root->max = new_node;
	} else {
	    old_ptr->left = new_node = avl_create_node();	/* AVL tree */
	    old_ptr->left_depth = 1;
	    new_node->next = old_ptr;				/* linked-list */
	    new_node->previous = last_right_node;
	    old_ptr->previous = new_node;
	    if( last_right_node )
		last_right_node->next = new_node;
	    else
		root->min = new_node;
	}
	new_node->parent = old_ptr;
	new_node->data = data;
	avl_adjust_depth( root , old_ptr->parent );
    }
    return new_node;
}


/*

  AVL-tree から削除

*/
avl_public void	AVL_delete( root , delete_node )
avl_root_node	*root;
avl_node	*delete_node;
{
    avl_node	*swap;
    avl_node	*adjust;

    if( delete_node == NULL ) {
	fputs( "AVL_delete: delete_node nil\n" , stderr );
	exit( 1 );
    }

    if( delete_node->left == NULL ) {
	if( delete_node->right == NULL ) {
	    /* 末端の葉 */
	    avl_adjust_parent( root , delete_node , NULL );
	    adjust = delete_node->parent;
	} else {
	    /* 右にだけ枝がある節 */
	    avl_adjust_parent( root , delete_node , delete_node->right );
	    delete_node->right->parent = delete_node->parent;
	    adjust = delete_node->parent;
	}
    } else {
	if( delete_node->right == NULL ) {
	    /* 左にだけ枝がある節 */
	    avl_adjust_parent( root , delete_node , delete_node->left );
	    delete_node->left->parent = delete_node->parent;
	    adjust = delete_node->parent;
	} else {
	    /* 左右両方に枝がある節 */
	    adjust = swap = delete_node->previous;
	    if( swap != delete_node->left ) {
		swap->parent->right = swap->left;
		if( swap->left )
		    swap->left->parent = swap->parent;
		swap->left = delete_node->left;
		swap->left->parent = swap;
		adjust = swap->parent;
	    }
	    avl_adjust_parent( root , delete_node , swap );
	    swap->parent = delete_node->parent;
	    swap->right = delete_node->right;
	    swap->right->parent = swap;
	}
    }

    if( delete_node->previous == NULL )
	root->min = delete_node->next;
    else
	delete_node->previous->next = delete_node->next;
    if( delete_node->next == NULL )
	root->max = delete_node->previous;
    else
	delete_node->next->previous = delete_node->previous;
    avl_adjust_depth( root , adjust );
    root->data_number--;
    (*( root->free_function ))( delete_node->data );
    free( delete_node );

    return;
}


/*

  AVL-tree から検索する

  見つからない場合 NULL を返す

*/
avl_public avl_node	*AVL_search( root , data )
avl_root_node	*root;
AVL_USERDATA	*data;
{
    avl_node	*node_ptr;
    int		comp;

    node_ptr = root->avl_tree;
    while( node_ptr != NULL ) {
	comp = AVL_COMPARE( data , node_ptr->data );
	if( comp < 0 )
	    node_ptr = node_ptr->left;
	else if( comp > 0 )
	    node_ptr = node_ptr->right;
	else
	    return node_ptr;	/* found */
    }
    return NULL;	/* not found */
}


/*

  AVL-tree から data の次を検索する

  data と同じ avl_node があればそれを返す

  見つからない場合 NULL を返す

*/
avl_public avl_node	*AVL_search_next( root , data )
avl_root_node	*root;
AVL_USERDATA	*data;
{
    avl_node	*node_ptr , *old_ptr = NULL;
    int		comp = 0;

    node_ptr = root->avl_tree;
    while( node_ptr != NULL ) {
	old_ptr = node_ptr;
	comp = AVL_COMPARE( data , node_ptr->data );
	if( comp < 0 )
	    node_ptr = node_ptr->left;
	else if( comp > 0 )
	    node_ptr = node_ptr->right;
	else
	    return node_ptr;	/* found */
    }
    /* not found */
    if( old_ptr ) {
	if( comp > 0 )
	    return old_ptr->next;
	else
	    return old_ptr;
    }
    return NULL;
}


/*

  AVL-tree から data の前を検索する

  data と同じ avl_node があればそれを返す

  見つからない場合 NULL を返す

*/
avl_public avl_node	*AVL_search_previous( root , data )
avl_root_node	*root;
AVL_USERDATA	*data;
{
    avl_node	*node_ptr , *old_ptr = NULL;
    int		comp = 0;

    node_ptr = root->avl_tree;
    while( node_ptr != NULL ) {
	old_ptr = node_ptr;
	comp = AVL_COMPARE( data , node_ptr->data );
	if( comp < 0 )
	    node_ptr = node_ptr->left;
	else if( comp > 0 )
	    node_ptr = node_ptr->right;
	else
	    return node_ptr;	/* found */
    }
    /* not found */
    if( old_ptr ) {
	if( comp > 0 )
	    return old_ptr;
	else
	    return old_ptr->previous;
    }
    return NULL;
}


/*

  AVL-tree を表示する

*/
avl_public void		AVL_print_tree( root )
avl_root_node	*root;
{
    avl_print_tree( root , root->avl_tree , 0 );
}


avl_private void	avl_print_tree( root , node_ptr , level )
avl_root_node	*root;
avl_node	*node_ptr;
int		level;
{
    int		i;

    if( node_ptr == NULL )
	return;
    avl_print_tree( root , node_ptr->right , level + 1 );
    for( i = level * 4 ; i > 0 ; i-- )
	putchar( ' ' );
    (*( root->print_function ))( node_ptr->data );
    printf( "(%d,%d)\n" , node_ptr->left_depth , node_ptr->right_depth );

    avl_print_tree( root , node_ptr->left , level + 1 );
}


#if 0
/*

  AVL-tree を表示する その２

*/
avl_public void		AVL_print_tree2( avl_root_node *root )
{
    avl_node	*node_ptr;

    printf( "\n" "--------\n" );

    for( node_ptr = AVL_get_max( root ); node_ptr != NULL;
	 node_ptr = AVL_previous( node_ptr ) ) {

	int level;
	avl_node  *nptr = node_ptr;

	/* node_ptr のレベルを調べる */
	for( level = 0; nptr != NULL; level++ )
	    nptr = nptr->parent;

	for( level *= 2; level > 0; level-- )
	    putchar(' ');
	(*( root->print_function ))( node_ptr->data );
	printf( "(%d,%d)\n", node_ptr->left_depth, node_ptr->right_depth );
    }
}
#endif


avl_public void		AVL_check_tree( root )
avl_root_node	*root;
{
    avl_check_tree( root , root->avl_tree );
}


avl_private int	avl_check_tree( root , node_ptr )
avl_root_node	*root;
avl_node	*node_ptr;
{
    int		left_depth , right_depth;

    if( node_ptr == NULL )
	return 0;

    left_depth = avl_check_tree( root , node_ptr->left );
    right_depth = avl_check_tree( root , node_ptr->right );
    if( AVL_BALANCE( node_ptr ) != 0 &&
        AVL_BALANCE( node_ptr ) != 1 &&
        AVL_BALANCE( node_ptr ) != -1 ) {
	fputs( "AVL_check_tree: tree not balanced\n" , stderr );
	exit( 1 );
    }
    if( ( node_ptr->left  && AVL_COMPARE( node_ptr->left->data  , node_ptr->data ) >= 0 ) ||
        ( node_ptr->right && AVL_COMPARE( node_ptr->data , node_ptr->right->data ) >= 0 ) ) {
	fputs( "AVL_check_tree: order error\n" , stderr );
	if( node_ptr->left ) {
	    fputs( "left node" , stderr );
	    (*( root->print_function ))( node_ptr->left->data );
	}
	fputc( ' ' , stderr );
	(*( root->print_function ))( node_ptr->data );
	fputc( ' ' , stderr );
	if( node_ptr->right ) {
	    fputs( "right node" , stderr );
	    (*( root->print_function ))( node_ptr->right->data );
	}
	fputc( '\n' , stderr );
	AVL_print_tree( root );
	exit(1);
    }
    if( node_ptr->left_depth != left_depth || node_ptr->right_depth != right_depth ) {
	fputs( "AVL_check_tree: depth error\n" , stderr );
	exit( 1 );
    }
    if( ( node_ptr->left  && node_ptr->left->parent != node_ptr )
     || ( node_ptr->right && node_ptr->right->parent != node_ptr ) ) {
	fputs( "AVL_check_tree: child error\n" , stderr );
	exit( 1 );
    }
    if( node_ptr->parent == NULL ) {
	if( root->avl_tree != node_ptr ) {
	    fputs( "AVL_check_tree: root node mismatch\n" , stderr );
	    exit( 1 );
	}
    } else {
	if( node_ptr->parent->left != node_ptr && node_ptr->parent->right != node_ptr ) {
	    fputs( "AVL_check_tree: parent error\n" , stderr );
	    exit( 1 );
	}
    }
    return( left_depth > right_depth ? left_depth : right_depth ) + 1;
}

/* EOF */
