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
 **功    能: 插入哈希数组
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
int hash_array_insert(hash_array_t *hash, int key, void *addr)
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
