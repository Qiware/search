/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: hash_array.c
 ** 版本号: 1.0
 ** 描  述: 哈希数组模块
 **         1. 使用哈希数组存储数据
 **         2. 使用红黑树解决数据冲突
 ** 作  者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "hash_array.h"

/******************************************************************************
 **函数名称: hash_array_init
 **功    能: 构建哈希数组
 **输入参数:
 **     num: 数组长度
 **     key: 生成KEY的函数
 **输出参数: NONE
 **返    回: 哈希数组地址
 **实现描述: 
 **     1. 创建哈希对象
 **     2. 创建内存池
 **     3. 创建数组空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
hash_array_t *hash_array_init(int num, uint32_t (*key)(const char *str, size_t len))
{
    int ret;
    hash_array_t *hash;
        

    /* 1. 创建哈希数组 */
    hash = (hash_array_t *)calloc(1, sizeof(hash_array_t));
    if (NULL == hash)
    {
        log2_error("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 创建内存池 */
    ret = eslab_init(&hash->eslab, 32 * KB);
    if (0 != ret)
    {
        free(hash);
        log2_error("Initialize slab failed!");
        return NULL;
    }

    /* 3. 创建数组空间 */
    hash->node = (hash_node_t *)eslab_alloc(&hash->eslab, num * sizeof(hash_node_t));
    if (NULL == hash->node)
    {
        eslab_destroy(&hash->eslab);
        free(hash);
        log2_error("Alloc memory from slab failed!");
        return NULL;
    }

    for (idx=0; idx<num; ++idx)
    {
        hash->node[idx].tree = NULL;
    }

    hash->key = key;

    return hash;
}

/******************************************************************************
 **函数名称: hash_array_insert
 **功    能: 插入哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     key: KEY值
 **     data: 数据
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
int hash_array_insert(hash_array_t *hash, int key, void *data)
{
    int idx;

    idx = key % hash->num;
    if (NULL == hash->node[idx].tree)
    {
        hash->node[idx].tree = rbt_creat();
        if (NULL == hash->node[idx].tree)
        {
            return -1;
        }
    }

    return rbt_insert(hash->node[idx].tree, key);
}

/******************************************************************************
 **函数名称: hash_array_search
 **功    能: 查找哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     key: KEY值
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
void *hash_array_search(hash_array_t *hash, int key)
{
    int idx;
    rbt_node_t *node;

    idx = key % hash->num;
    if (NULL == hash->node[idx].tree)
    {
        return NULL;
    }

    node = rbt_search(hash->node[idx].tree, key);
    if (NULL == node)
    {
        return NULL;
    }

    return node->data;
}

/******************************************************************************
 **函数名称: hash_array_delete
 **功    能: 删除哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     key: KEY值
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **     注意: 返回地址的内存空间由外部释放
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
void *hash_array_delete(hash_array_t *hash, int key)
{
    int idx;
    void *data;
    rbt_node_t *node;

    idx = key % hash->num;
    if (NULL == hash->node[idx].tree)
    {
        return NULL;
    }

    node = rbt_search(hash->node[idx].tree, key);
    if (NULL == node)
    {
        return NULL;
    }

    data = node->data;

    rbt_delete(hash->node[idx].tree, key);

    return data;
}

/******************************************************************************
 **函数名称: hash_array_destroy
 **功    能: 销毁哈希数组(未完成)
 **输入参数:
 **     hash: 哈希数组
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     注意: 该函数还未完成: 未释放DATA空间
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
int hash_array_destroy(hash_array_t *hash)
{
    int idx;

    for (idx=0; idx<hash->num; ++idx)
    {
        if (NULL == hash->node[idx].tree)
        {
            continue;
        }

        rbt_destroy(&hash->node[idx].tree);
    }

    eslab_destroy(&hash->eslab);
    free(hash);

    return 0;
}
