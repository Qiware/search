/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: kw_tree.c
 ** 版本号: 1.0
 ** 描  述: KW树的实现
 ** 作  者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
#include "comm.h"
#include "kw_tree.h"

static void kwt_node_free(kwt_tree_t *tree, kwt_node_t *node, void *mempool, mem_dealloc_cb_t dealloc);

/******************************************************************************
 **函数名称: kwt_creat
 **功    能: 创建KW树
 **输入参数: NONE
 **输出参数: NONE
 **返    回: KW树
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
kwt_tree_t *kwt_creat(void)
{
    int max = 256;
    kwt_tree_t *tree;

    if (!ISPOWEROF2(max)) { return NULL; }

    /* 1. 创建对象 */
    tree = (kwt_tree_t *)calloc(1, sizeof(kwt_tree_t));
    if (NULL == tree)
    {
        return NULL;
    }

    /* 2. 创建结点 */
    tree->max = max;
    tree->root = (kwt_node_t *)calloc(max, sizeof(kwt_node_t));
    if (NULL == tree->root)
    {
        free(tree);
        return NULL;
    }

    return tree; 
}

/******************************************************************************
 **函数名称: kwt_insert
 **功    能: 插入字符串
 **输入参数:
 **     tree: KW树
 **     str: 字串(各字符取值：0x00~0XFF)
 **     len: 字串长度
 **     data: 附加数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
int kwt_insert(kwt_tree_t *tree, const unsigned char *str, int len, void *data)
{
    int i, max = len - 1;
    kwt_node_t *node = tree->root;

    if (len <= 0) { return -1; }

    for (i=0; i<len; ++i)
    {
        node += str[i];
        node->key = str[i];
        if ((i < max) && (NULL == node->child))
        {
            node->child = (kwt_node_t *)calloc(tree->max, sizeof(kwt_node_t));
            if (NULL == node->child)
            {
                return -1;
            }
        }
        node = node->child;
    }

    return 0;
}

/******************************************************************************
 **函数名称: kwt_search
 **功    能: 搜索字符串
 **输入参数:
 **     tree: KW树
 **     str: 字串(各字符取值：0x00~0XFF)
 **     len: 字串长度
 **输出参数:
 **     data: 附加参数
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
int kwt_search(kwt_tree_t *tree, const unsigned char *str, int len, void **data)
{
    int i, max = len - 1;
    kwt_node_t *node = tree->root;

    /* 1. 搜索KW树 */
    for (i=0; i<len; ++i)
    {
        node += str[i];
        if (node->key != str[i])
        {
            *data = NULL;
            return -1;
        }
        else if (i == max)
        {
            *data = node->data;
            return 0;
        }

        node = node->child;
    }

    *data = NULL;
    return -1;
}

/******************************************************************************
 **函数名称: kwt_destroy
 **功    能: 销毁KW树
 **输入参数: 
 **     tree: KW树
 **     mempool: 附加数据的内存池
 **     dealloc: 释放附加数据空间的毁掉函数
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
void kwt_destroy(kwt_tree_t *tree, void *mempool, mem_dealloc_cb_t dealloc)
{
    if (NULL != tree->root)
    {
        kwt_node_free(tree, tree->root, mempool, dealloc);
    }

    free(tree);
}

/******************************************************************************
 **函数名称: kwt_node_free
 **功    能: 销毁Trie结点
 **输入参数: 
 **     tree: KW树
 **     node: KW树结点
 **     mempool: 附加数据的内存池
 **     dealloc: 释放附加数据空间的毁掉函数
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.05.12 #
 ******************************************************************************/
static void kwt_node_free(kwt_tree_t *tree, kwt_node_t *node, void *mempool, mem_dealloc_cb_t dealloc)
{
    int i;

    for (i=0;i<tree->max; ++i)
    {
        if (NULL == node[i].child)
        {
            continue;
        }
        dealloc(mempool, node[i].data);
        kwt_node_free(tree, node[i].child, mempool, dealloc);
        node[i].child = NULL;
    }

    free(node);
}
