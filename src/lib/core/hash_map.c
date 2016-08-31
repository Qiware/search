/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: hash_map.c
 ** 版本号: 1.0
 ** 描  述: 哈希表模块
 **         1. 使用哈希数组分解锁的压力
 **         2. 使用红黑树解决数据查找的性能问题
 ** 作  者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
#include "rb_tree.h"
#include "avl_tree.h"
#include "hash_map.h"

/******************************************************************************
 **函数名称: hash_map_creat
 **功    能: 创建哈希表
 **输入参数:
 **     len: 哈希表长度
 **     key: 生成KEY的函数
 **     cmp: 数据比较函数
 **     opt: 其他选项
 **输出参数: NONE
 **返    回: 哈希数组地址
 **实现描述:
 **     1. 创建哈希对象
 **     2. 创建内存池
 **     3. 创建数组空间
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
hash_map_t *hash_map_creat(int len,
        key_cb_t key_cb, cmp_cb_t cmp_cb, hash_map_opt_t *opt)
{
    int idx;
    hash_map_t *htab;
    rbt_opt_t rbt_opt;

    /* > 创建哈希数组 */
    htab = (hash_map_t *)opt->alloc(opt->pool, sizeof(hash_map_t));
    if (NULL == htab) {
        return NULL;
    }

    htab->total = 0;

    htab->tree = (void **)opt->alloc(opt->pool, len*sizeof(void *));
    if (NULL == htab->tree) {
        opt->dealloc(opt->pool, htab);
        return NULL;
    }

    htab->lock = (pthread_rwlock_t *)opt->alloc(opt->pool, len*sizeof(pthread_rwlock_t));
    if (NULL == htab->lock) {
        opt->dealloc(opt->pool, htab->tree);
        opt->dealloc(opt->pool, htab);
        return NULL;
    }

    /* > 初始化数组 */
    memset(&rbt_opt, 0, sizeof(rbt_opt));

    rbt_opt.pool = (void *)opt->pool;
    rbt_opt.alloc = (mem_alloc_cb_t)opt->alloc;
    rbt_opt.dealloc = (mem_dealloc_cb_t)opt->dealloc;

    for (idx=0; idx<len; ++idx) {
        pthread_rwlock_init(&htab->lock[idx], NULL);

        htab->tree[idx] = rbt_creat(&rbt_opt, cmp_cb);
        if (NULL == htab->tree[idx]) {
            hash_map_destroy(htab, mem_dummy_dealloc, NULL);
            return NULL;
        }
    }

    /* > 注册回调函数 */
    htab->len = len;
    htab->key_cb = key_cb;
    htab->cmp_cb = cmp_cb;

    htab->insert = (tree_insert_cb_t)rbt_insert;
    htab->delete = (tree_delete_cb_t)rbt_delete;
    htab->query = (tree_query_cb_t)rbt_query;
    htab->trav = (tree_trav_cb_t)rbt_trav;
    htab->destroy = (tree_destroy_cb_t)rbt_destroy;

    return htab;
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
int hash_map_insert(hash_map_t *htab, void *data)
{
    int ret;
    unsigned int idx;

    idx = htab->key_cb(data) % htab->len;

    pthread_rwlock_wrlock(&htab->lock[idx]);
    ret = htab->insert(htab->tree[idx], data);
    if (AVL_OK == ret) {
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
int hash_map_query(hash_map_t *htab, void *key, copy_cb_t copy, void *data)
{
    int ret;
    void *orig;
    unsigned int idx;

    idx = htab->key_cb(key) % htab->len;

    pthread_rwlock_rdlock(&htab->lock[idx]);
    orig = htab->query((void *)htab->tree[idx], key);
    if (NULL == orig) {
        pthread_rwlock_unlock(&htab->lock[idx]);
        return -1; /* 未找到 */
    }

    ret = copy(orig, data);

    pthread_rwlock_unlock(&htab->lock[idx]);

    return ret;
}

/******************************************************************************
 **函数名称: hash_map_delete
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
void *hash_map_delete(hash_map_t *htab, void *key)
{
    void *data;
    unsigned int idx;

    idx = htab->key_cb(key) % htab->len;

    pthread_rwlock_wrlock(&htab->lock[idx]);
    htab->delete(htab->tree[idx], key, &data);
    if (NULL != data) {
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

    for (idx=0; idx<htab->len; ++idx) {
        pthread_rwlock_wrlock(&htab->lock[idx]);
        if (NULL != htab->tree[idx]) {
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

    for (idx=0; idx<htab->len; ++idx) {
        pthread_rwlock_rdlock(&htab->lock[idx]);
        htab->trav(htab->tree[idx], proc, args);
        pthread_rwlock_unlock(&htab->lock[idx]);
    }

    return 0;
}
