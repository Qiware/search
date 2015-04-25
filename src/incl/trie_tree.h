#if !defined(__TRIE_TREE_H__)
#define __TRIE_TREE_H__

#include "comm.h"

/* TRIE树的结点 */
typedef struct _trie_tree_node_t
{
    void *data;                         /* 结点数据 */
    struct _trie_tree_node_t *next;     /* 后一个结点 */
} trie_tree_node_t;

/* TRIE树 */
typedef struct
{
    int count;                          /* Unit中结点个数 */
    trie_tree_node_t *root;             /* TRIE树根结点 */
} trie_tree_t;

trie_tree_t *trie_tree_creat(int count);
int trie_tree_insert(trie_tree_t *tree, const char *str, int len);
const trie_tree_node_t *trie_tree_search(trie_tree_t *tree, const char *str, int len);
void trie_tree_destroy(trie_tree_t **tree);
#endif /*__TRIE_TREE_H__*/
