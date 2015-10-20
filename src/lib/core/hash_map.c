/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: hash_map.c
 ** 版本号: 1.0
 ** 描  述: 哈希表模块
 **         1. 使用哈希数组分解锁的压力
 **         2. 使用平衡二叉树解决数据查找的性能问题
 **         3. TODO: 可使用红黑树、链表等操作回调复用该框架!
 ** 作  者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
#include "rb_tree.h"
#include "avl_tree.h"
#include "hash_map.h"

static int hash_map_set_cb(hash_map_t *htab, hash_map_opt_t *opt);
static int hash_map_init_elem(hash_map_t *htab, int idx, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt);

/******************************************************************************
 **函数名称: hash_map_creat
 **功    能: 创建哈希表
 **输入参数:
 **     len: 哈希表长度
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
hash_map_t *hash_map_creat(int len, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt)
{
    int idx;
    hash_map_t *htab;

    /* > 创建哈希数组 */
    htab = (hash_map_t *)opt->alloc(opt->pool, sizeof(hash_map_t));
    if (NULL == htab)
    {
        return NULL;
    }

    htab->total = 0;

    htab->tree = (void **)opt->alloc(opt->pool, len*sizeof(void *));
    if (NULL == htab->tree)
    {
        opt->dealloc(opt->pool, htab);
        return NULL;
    }

    htab->lock = (pthread_rwlock_t *)opt->alloc(opt->pool, len*sizeof(pthread_rwlock_t));
    if (NULL == htab->lock)
    {
        opt->dealloc(opt->pool, htab->tree);
        opt->dealloc(opt->pool, htab);
        return NULL;
    }

    /* > 创建存储树 */
    hash_map_set_cb(htab, opt);

    for (idx=0; idx<len; ++idx)
    {
        pthread_rwlock_init(&htab->lock[idx], NULL);

        if (hash_map_init_elem(htab, idx, key_cb, cmp_cb, opt))
        {
            hash_map_destroy(htab, mem_dummy_dealloc, NULL);
            return NULL;
        }
    }

    htab->len = len;
    htab->key_cb = key_cb;
    htab->cmp_cb = cmp_cb;

    return htab;
}

/* 设置回调函数 */
static int hash_map_set_cb(hash_map_t *htab, hash_map_opt_t *opt)
{
    switch (opt->type)
    {
        case HASH_MAP_AVL:
        default:
        {
            htab->tree_insert = (tree_insert_cb_t)avl_insert;
            htab->tree_delete = (tree_delete_cb_t)avl_delete;
            htab->tree_query = (tree_query_cb_t)avl_query;
            htab->tree_trav = (tree_trav_cb_t)avl_trav;
            htab->tree_destroy = (tree_destroy_cb_t)avl_trav;
            break;
        }
        case HASH_MAP_RBT:
        {
            htab->tree_insert = (tree_insert_cb_t)rbt_insert;
            htab->tree_delete = (tree_delete_cb_t)rbt_delete;
            htab->tree_query = (tree_query_cb_t)rbt_query;
            htab->tree_trav = (tree_trav_cb_t)rbt_trav;
            htab->tree_destroy = (tree_destroy_cb_t)rbt_trav;
            break;
        }
    }
    return 0;
}

/******************************************************************************
 **函数名称: hash_map_init_elem
 **功    能: 初始化哈希数组成员
 **输入参数:
 **     tab: 哈希表
 **     key_cb: 键值回调
 **     cmp_cb: 比较回调
 **     opt: 其他选项
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015-07-22 14:23:04 #
 ******************************************************************************/
static int hash_map_init_elem(
    hash_map_t *htab, int idx, key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt)
{
    switch (opt->type)
    {
        case HASH_MAP_AVL:
        default:
        {
            avl_opt_t avl_opt;

            memset(&avl_opt, 0, sizeof(avl_opt));

            avl_opt.pool = (void *)opt->pool;
            avl_opt.alloc = (mem_alloc_cb_t)opt->alloc;
            avl_opt.dealloc = (mem_dealloc_cb_t)opt->dealloc;

            htab->tree[idx] = avl_creat(&avl_opt, key_cb, cmp_cb);
            if (NULL == htab->tree[idx])
            {
                return -1;
            }
            break;
        }
        case HASH_MAP_RBT:
        {
            rbt_opt_t rbt_opt;

            memset(&rbt_opt, 0, sizeof(rbt_opt));

            rbt_opt.pool = (void *)opt->pool;
            rbt_opt.alloc = (mem_alloc_cb_t)opt->alloc;
            rbt_opt.dealloc = (mem_dealloc_cb_t)opt->dealloc;

            htab->tree[idx] = rbt_creat(&rbt_opt, key_cb, cmp_cb);
            if (NULL == htab->tree[idx])
            {
                return -1;
            }
            break;
        }
    }
    return 0;
}

/******************************************************************************
 **函数名称: hash_map_insert
 **功    能: 插入哈希成员
 **输入参数:
 **     htab: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **输出参数:
 **     data: 数据
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_map_insert(hash_map_t *htab, void *pkey, int pkey_len, void *data)
{
    int ret;
    unsigned int idx;

    idx = htab->key_cb(pkey, pkey_len) % htab->len;

    pthread_rwlock_wrlock(&htab->lock[idx]);
    ret = htab->tree_insert(htab->tree[idx], pkey, pkey_len, data);
    if (AVL_OK == ret)
    {
        ++htab->total;
    }
    pthread_rwlock_unlock(&htab->lock[idx]);

    return ret;
}

/******************************************************************************
 **函数名称: hash_map_query
 **功    能: 查找哈希成员
 **输入参数:
 **     htab: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **     query_cb: 查找函数
 **输出参数:
 **     data: 查找结果
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_map_query(hash_map_t *htab,
    void *pkey, int pkey_len, hash_map_query_cb_t query_cb, void *data)
{
    int ret;
    void *orig;
    unsigned int idx;

    idx = htab->key_cb(pkey, pkey_len) % htab->len;

    pthread_rwlock_rdlock(&htab->lock[idx]);
    orig = avl_query(htab->tree[idx], pkey, pkey_len);
    if (NULL == orig)
    {
        pthread_rwlock_unlock(&htab->lock[idx]);
        return -1; /* 未找到 */
    }

    ret = query_cb(orig, data);

    pthread_rwlock_unlock(&htab->lock[idx]);

    return ret;
}

/******************************************************************************
 **函数名称: hash_map_remove
 **功    能: 删除哈希成员
 **输入参数:
 **     htab: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述:
 **注意事项: 返回地址的内存空间由外部释放
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
void *hash_map_remove(hash_map_t *htab, void *pkey, int pkey_len)
{
    void *data;
    unsigned int idx;

    idx = htab->key_cb(pkey, pkey_len) % htab->len;

    pthread_rwlock_wrlock(&htab->lock[idx]);
    htab->tree_delete(htab->tree[idx], pkey, pkey_len, &data);
    if (NULL != data)
    {
        --htab->total;
    }
    pthread_rwlock_unlock(&htab->lock[idx]);

    return data;
}

/******************************************************************************
 **函数名称: hash_map_destroy
 **功    能: 销毁哈希数组
 **输入参数:
 **     htab: 哈希数组
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_map_destroy(hash_map_t *htab, mem_dealloc_cb_t dealloc, void *args)
{
    int idx;

    for (idx=0; idx<htab->len; ++idx)
    {
        pthread_rwlock_wrlock(&htab->lock[idx]);
        if (NULL != htab->tree[idx])
        {
            avl_destroy(htab->tree[idx], dealloc, args);
        }
        pthread_rwlock_unlock(&htab->lock[idx]);

        pthread_rwlock_destroy(&htab->lock[idx]);
    }

    htab->dealloc(htab->pool, htab->tree);
    htab->dealloc(htab->pool, htab->lock);
    htab->dealloc(htab->pool, htab);

    return 0;
}

/******************************************************************************
 **函数名称: hash_map_trav
 **功    能: 遍历哈希数组
 **输入参数:
 **     htab: 哈希数组
 **     proc: 回调函数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
int hash_map_trav(hash_map_t *htab, trav_cb_t proc, void *args)
{
    int idx;

    for (idx=0; idx<htab->len; ++idx)
    {
        pthread_rwlock_rdlock(&htab->lock[idx]);
        htab->tree_trav(htab->tree[idx], proc, args);
        pthread_rwlock_unlock(&htab->lock[idx]);
    }

    return 0;
}
