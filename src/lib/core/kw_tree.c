/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: kw_tree.c
 ** 版本号: 1.0
 ** 描  述: KW树的实现
 **
 **  -----------------------------------
 ** |  0x00  |  ....  |  ....  |  0xFF  |
 **  -----------------------------------
 **                  /
 **                 /
 **                 -----------------------------------
 **                |  0x00  |  ....  |  ....  |  0xFF  |
 **                 -----------------------------------
 **                        /
 **                       /
 **                       -----------------------------------
 **                      |  0x00  |  ....  |  ....  |  0xFF  |
 **                       -----------------------------------
 ** 作  者: # Qifeng.zou # 20104.09.01 #
 ******************************************************************************/
#include "kw_tree.h"

static void kwt_node_free(kwt_tree_t *tree, kwt_node_t *node);

/******************************************************************************
 **函数名称: kwt_creat
 **功    能: 创建KW树
 **输入参数: NONE
 **输出参数: NONE
 **返    回: KW树
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
kwt_tree_t *kwt_creat(int count)
{
    kwt_tree_t *tree;

    /* 1. 创建对象 */
    tree = (kwt_tree_t *)calloc(1, sizeof(kwt_tree_t));
    if (NULL == tree)
    {
        return NULL;
    }

    /* 2. 创建结点 */
    tree->count = count;
    tree->root = (kwt_node_t *)calloc(tree->count, sizeof(kwt_node_t));
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
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
int kwt_insert(kwt_tree_t *tree, const char *str, int len)
{
    int idx, max = len - 1;
    kwt_node_t *node = tree->root;

    /* 1. 构建KW树 */
    for (idx=0; idx<max; ++idx)
    {
        if (NULL == node[(uint8_t)str[idx]].next)
        {
            node[(uint8_t)str[idx]].next =
                (kwt_node_t *)calloc(tree->count, sizeof(kwt_node_t));
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
 **函数名称: kwt_search
 **功    能: 搜索字符串
 **输入参数:
 **     tree: KW树
 **     str: 字串(各字符取值：0x00~0XFF)
 **     len: 字串长度
 **输出参数: NONE
 **返    回: 结点地址
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
const kwt_node_t *kwt_search(kwt_tree_t *tree, const char *str, int len)
{
    int idx, max = len - 1;
    kwt_node_t *node = tree->root;

    /* 1. 搜索KW树 */
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
 **函数名称: kwt_destroy
 **功    能: 销毁KW树
 **输入参数: 
 **     tree: KW树
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
void kwt_destroy(kwt_tree_t **tree)
{
    if (NULL != (*tree)->root)
    {
        kwt_node_free(*tree, (*tree)->root);
    }

    free(*tree), *tree = NULL;
}

/******************************************************************************
 **函数名称: kwt_node_free
 **功    能: 销毁Trie结点
 **输入参数: 
 **     tree: KW树
 **     node: KW树结点
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: TODO: 未释放结点DATA内存
 **作    者: # Qifeng.zou # 2014.09.01 #
 ******************************************************************************/
static void kwt_node_free(kwt_tree_t *tree, kwt_node_t *node)
{
    int idx;

    for (idx=0;idx<tree->count; ++idx)
    {
        if (NULL == node[idx].next)
        {
            continue;
        }

        kwt_node_free(tree, node[idx].next);

        node[idx].next = NULL;
    }

    free(node);
}
