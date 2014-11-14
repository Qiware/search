/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: hash_tab.c
 ** 版本号: 1.0
 ** 描  述: 哈希表模块
 **         1. 使用哈希数组分解锁的压力
 **         2. 使用平衡树解决数据查找的性能问题
 ** 作  者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"
#include "hash_tab.h"

/******************************************************************************
 **函数名称: hash_tab_creat
 **功    能: 创建哈希表
 **输入参数:
 **     num: 数组长度
 **     key: 生成KEY的函数
 **     cmp: 数据比较函数
 **输出参数: NONE
 **返    回: 哈希数组地址
 **实现描述: 
 **     1. 创建哈希对象
 **     2. 创建内存池
 **     3. 创建数组空间
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
hash_tab_t *hash_tab_creat(int num, avl_key_cb_t key_cb, avl_cmp_cb_t cmp_cb)
{
    int idx;
    hash_tab_t *hash;
        

    /* 1. 创建哈希数组 */
    hash = (hash_tab_t *)calloc(1, sizeof(hash_tab_t));
    if (NULL == hash)
    {
        log2_error("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 创建数组空间 */
    hash->tree = (avl_tree_t **)calloc(num, sizeof(avl_tree_t *));
    if (NULL == hash->tree)
    {
        free(hash);
        log2_error("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    hash->lock = (pthread_rwlock_t *)calloc(num, sizeof(pthread_rwlock_t));
    if (NULL == hash->lock)
    {
        free(hash->tree);
        free(hash);
        log2_error("errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 3. 创建平衡二叉树 */
    for (idx=0; idx<num; ++idx)
    {
        pthread_rwlock_init(&hash->lock[idx], NULL);

        if (0 != avl_creat(&hash->tree[idx], key_cb, cmp_cb))
        {
            free(hash->tree);
            free(hash);
            log2_error("Create avl tree failed!");
            return NULL;
        }

        hash->tree[idx]->key_cb = key_cb;
        hash->tree[idx]->cmp_cb = cmp_cb;
    }

    hash->num = num;
    hash->key_cb = key_cb;
    hash->cmp_cb = cmp_cb;

    return hash;
}

/******************************************************************************
 **函数名称: hash_tab_insert
 **功    能: 插入哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **输出参数:
 **     data: 数据
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_tab_insert(hash_tab_t *hash, void *pkey, int pkey_len, void *data)
{
    int ret;
    uint32_t idx;

    idx = hash->key_cb(pkey, pkey_len) % hash->num;

    pthread_rwlock_wrlock(&hash->lock[idx]);
    ret = avl_insert(hash->tree[idx], pkey, pkey_len, data);
    pthread_rwlock_unlock(&hash->lock[idx]);

    return ret;
}

/******************************************************************************
 **函数名称: hash_tab_query
 **功    能: 查找哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **     data_len: 结果取长度
 **输出参数:
 **     data: 查找结果
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_tab_query(hash_tab_t *hash, void *pkey, int pkey_len, void *data, int data_len)
{
    uint32_t idx;
    avl_node_t *node;


    idx = hash->key_cb(pkey, pkey_len) % hash->num;

    pthread_rwlock_rdlock(&hash->lock[idx]);
    node = avl_query(hash->tree[idx], pkey, pkey_len);
    if (NULL == node)
    {
        pthread_rwlock_unlock(&hash->lock[idx]);
        return -1; /* 未找到 */
    }

    memcpy(data, node->data, data_len);
    pthread_rwlock_unlock(&hash->lock[idx]);

    return 0;
}

/******************************************************************************
 **函数名称: hash_tab_delete
 **功    能: 删除哈希成员
 **输入参数:
 **     hash: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **     注意: 返回地址的内存空间由外部释放
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
void *hash_tab_delete(hash_tab_t *hash, void *pkey, int pkey_len)
{
    void *data;
    uint32_t idx;


    idx = hash->key_cb(pkey, pkey_len) % hash->num;

    pthread_rwlock_wrlock(&hash->lock[idx]);
    avl_delete(hash->tree[idx], pkey, pkey_len, &data);
    pthread_rwlock_unlock(&hash->lock[idx]);

    return data;
}

/******************************************************************************
 **函数名称: hash_tab_destroy
 **功    能: 销毁哈希数组(未完成)
 **输入参数:
 **     hash: 哈希数组
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **     注意: 该函数还未完成: 未释放DATA空间
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_tab_destroy(hash_tab_t *hash)
{
    uint32_t idx;

    for (idx=0; idx<hash->num; ++idx)
    {
        avl_destroy(&hash->tree[idx]);
    }

    free(hash);

    return 0;
}
