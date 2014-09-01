/******************************************************************************
 ** Coypright(C) 2013-2014 Xundao technology Co., Ltd
 **
 ** 文件名: trie_tree.c
 ** 版本号: 1.0
 ** 描  述: Trie树的实现
 ** 作  者: # Qifeng.zou # 20104.09.01 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "trie_tree.h"

static void trie_tree_node_free(trie_tree_t *tree, trie_tree_node_t *node);

/******************************************************************************
 **函数名称: trie_tree_creat
 **功    能: 创建Trie树
 **输入参数: NONE
 **输出参数: NONE
 **返    回: Trie树
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
trie_tree_t *trie_tree_creat(int count)
{
    trie_tree_t *tree;

    /* 1. 创建对象 */
    tree = (trie_tree_t *)calloc(1, sizeof(trie_tree_t));
    if (NULL == tree)
    {
        return NULL;
    }

    /* 2. 创建结点 */
    tree->count = count;
    tree->root = (trie_tree_node_t *)calloc(
                    tree->count, sizeof(trie_tree_node_t));
    if (NULL == tree->root)
    {
        free(tree);
        return NULL;
    }

    return tree; 
}

/******************************************************************************
 **函数名称: trie_tree_insert
 **功    能: 插入字符串
 **输入参数:
 **     tree: Trie树
 **     str: 字串(各字符取值：0x00~0XFF)
 **     len: 字串长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
int trie_tree_insert(trie_tree_t *tree, const char *str, int len)
{
    int idx = 0, max = len - 1;
    trie_tree_node_t *node = tree->root;

    /* 1. 构建Trie树 */
    for (idx=0; idx<max; ++idx)
    {
        if (NULL == node[(uint8_t)str[idx]].next)
        {
            node[(uint8_t)str[idx]].next =
                (trie_tree_node_t *)calloc(tree->count, sizeof(trie_tree_node_t));
            if (NULL == node[(uint8_t)str[idx]].next)
            {
                return -1;
            }
        }

        node = node[(uint8_t)str[idx]].next;
    }

    /* 2. 插入数据信息(待续) */
    if (NULL == node[(uint8_t)str[len-1]].data)
    {
        return 0;
    }

    return 0;
}

/******************************************************************************
 **函数名称: trie_tree_search
 **功    能: 搜索字符串
 **输入参数:
 **     tree: Trie树
 **     str: 字串(各字符取值：0x00~0XFF)
 **     len: 字串长度
 **输出参数: NONE
 **返    回: 结点地址
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
const trie_tree_node_t *trie_tree_search(trie_tree_t *tree, const char *str, int len)
{
    int idx = 0, max = len - 1;
    trie_tree_node_t *node = tree->root;

    /* 1. 搜索Trie树 */
    for (idx=0; idx<max; ++idx)
    {
        if (NULL == node[(uint8_t)str[idx]].next)
        {
            return NULL;
        }

        node = node[(uint8_t)str[idx]].next;
    }

    return &node[(uint8_t)str[len - 1]];
}

/******************************************************************************
 **函数名称: trie_tree_destroy
 **功    能: 销毁Trie树
 **输入参数: 
 **     tree: Trie树
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
void trie_tree_destroy(trie_tree_t **tree)
{
    if (NULL != (*tree)->root)
    {
        trie_tree_node_free(*tree, (*tree)->root);
    }

    free(*tree), *tree = NULL;
}

/******************************************************************************
 **函数名称: trie_tree_node_free
 **功    能: 销毁Trie结点
 **输入参数: 
 **     tree: Trie树
 **     node: Trie树结点
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 未完成：未释放结点DATA内存
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
static void trie_tree_node_free(trie_tree_t *tree, trie_tree_node_t *node)
{
    int idx;

    for (idx=0;idx<tree->count; ++idx)
    {
        if (NULL == node[idx].next)
        {
            continue;
        }

        trie_tree_node_free(tree, node[idx].next);

        node[idx].next = NULL;
    }

    free(node);
}
