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

static void _hash_map_lock(hash_map_t *hmap, int idx, lock_e lock)
{
    if (WRLOCK == lock) {
        pthread_rwlock_wrlock(&hmap->lock[idx]);
    }
    else if (RDLOCK == lock) {
        pthread_rwlock_rdlock(&hmap->lock[idx]);
    }
}

static void _hash_map_unlock(hash_map_t *hmap, int idx, lock_e lock)
{
    if ((WRLOCK == lock) || (RDLOCK == lock)) {
        pthread_rwlock_unlock(&hmap->lock[idx]);
    }
}

/******************************************************************************
 **函数名称: hash_map_unlock
 **功    能: 哈希表解锁
 **输入参数:
 **     map: 哈希表
 **     key: 主键
 **     lock: 解哪种锁(读锁/写锁)
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.09.10 04:19:51 #
 ******************************************************************************/
void hash_map_unlock(hash_map_t *hmap, void *key, lock_e lock)
{
    unsigned int idx;

    idx = hmap->key_cb(key) % hmap->len;

    if ((WRLOCK == lock) || (RDLOCK == lock)) {
        pthread_rwlock_unlock(&hmap->lock[idx]);
    }
}

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
    hash_map_t *hmap;
    rbt_opt_t rbt_opt;
    hash_map_opt_t hmap_opt;

    if (NULL == opt) {
        memset(&hmap_opt, 0, sizeof(hmap_opt));

        hmap_opt.pool = (void *)NULL;
        hmap_opt.alloc = (mem_alloc_cb_t)opt->alloc;
        hmap_opt.dealloc = (mem_dealloc_cb_t)opt->dealloc;

        opt = &hmap_opt;
    }

    /* > 创建哈希数组 */
    hmap = (hash_map_t *)opt->alloc(opt->pool, sizeof(hash_map_t));
    if (NULL == hmap) {
        return NULL;
    }

    hmap->total = 0;
    hmap->len = len;
    hmap->key_cb = key_cb;
    hmap->cmp_cb = cmp_cb;

    hmap->tree = (void **)opt->alloc(opt->pool, len*sizeof(void *));
    if (NULL == hmap->tree) {
        opt->dealloc(opt->pool, hmap);
        return NULL;
    }

    hmap->lock = (pthread_rwlock_t *)opt->alloc(opt->pool, len*sizeof(pthread_rwlock_t));
    if (NULL == hmap->lock) {
        opt->dealloc(opt->pool, hmap->tree);
        opt->dealloc(opt->pool, hmap);
        return NULL;
    }

    /* > 初始化数组 */
    memset(&rbt_opt, 0, sizeof(rbt_opt));

    rbt_opt.pool = (void *)opt->pool;
    rbt_opt.alloc = (mem_alloc_cb_t)opt->alloc;
    rbt_opt.dealloc = (mem_dealloc_cb_t)opt->dealloc;

    for (idx=0; idx<len; ++idx) {
        pthread_rwlock_init(&hmap->lock[idx], NULL);

        hmap->tree[idx] = rbt_creat(&rbt_opt, cmp_cb);
        if (NULL == hmap->tree[idx]) {
            hash_map_destroy(hmap, mem_dummy_dealloc, NULL);
            return NULL;
        }
    }

    return hmap;
}

/******************************************************************************
 **函数名称: hash_map_insert
 **功    能: 插入哈希成员
 **输入参数:
 **     hmap: 哈希数组
 **     data: 需要插入的数据
 **     lock: 锁操作
 **输出参数:
 **     data: 数据
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_map_insert(hash_map_t *hmap, void *data, lock_e lock)
{
    int ret;
    unsigned int idx;

    idx = hmap->key_cb(data) % hmap->len;

    _hash_map_lock(hmap, idx, lock);
    ret = rbt_insert(hmap->tree[idx], data);
    if (AVL_OK == ret) {
        ++hmap->total;
    }
    _hash_map_unlock(hmap, idx, lock);

    return ret;
}

/******************************************************************************
 **函数名称: hash_map_query
 **功    能: 查找哈希成员
 **输入参数:
 **     hmap: 哈希数组
 **     key: 主键
 **     lock: 锁操作
 **输出参数:
 **返    回: 查询的数据
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
void *hash_map_query(hash_map_t *hmap, void *key, lock_e lock)
{
    void *data;
    unsigned int idx;

    idx = hmap->key_cb(key) % hmap->len;

    _hash_map_lock(hmap, idx, lock);
    data = rbt_query((void *)hmap->tree[idx], key);
    if (NULL == data) {
        _hash_map_unlock(hmap, idx, lock);
        return NULL; /* 未找到 */
    }

    return data;
}

/******************************************************************************
 **函数名称: hash_map_delete
 **功    能: 删除哈希成员
 **输入参数:
 **     hmap: 哈希数组
 **     pkey: 主键
 **     pkey_len: 主键长度
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述:
 **注意事项: 返回地址的内存空间由外部释放
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
void *hash_map_delete(hash_map_t *hmap, void *key, lock_e lock)
{
    void *data;
    unsigned int idx;

    idx = hmap->key_cb(key) % hmap->len;

    _hash_map_lock(hmap, idx, lock);
    rbt_delete(hmap->tree[idx], key, &data);
    if (NULL != data) {
        --hmap->total;
    }
    _hash_map_unlock(hmap, idx, lock);

    return data;
}

/******************************************************************************
 **函数名称: hash_map_destroy
 **功    能: 销毁哈希数组
 **输入参数:
 **     hmap: 哈希数组
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_map_destroy(hash_map_t *hmap, mem_dealloc_cb_t dealloc, void *args)
{
    int idx;

    for (idx=0; idx<hmap->len; ++idx) {
        pthread_rwlock_wrlock(&hmap->lock[idx]);
        if (NULL != hmap->tree[idx]) {
            avl_destroy(hmap->tree[idx], dealloc, args);
        }
        pthread_rwlock_unlock(&hmap->lock[idx]);

        pthread_rwlock_destroy(&hmap->lock[idx]);
    }

    hmap->dealloc(hmap->pool, hmap->tree);
    hmap->dealloc(hmap->pool, hmap->lock);
    hmap->dealloc(hmap->pool, hmap);

    return 0;
}

/******************************************************************************
 **函数名称: hash_map_trav
 **功    能: 遍历哈希数组
 **输入参数:
 **     hmap: 哈希数组
 **     proc: 回调函数
 **     args: 附加参数
 **     lock: 加锁方式
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
int hash_map_trav(hash_map_t *hmap, trav_cb_t proc, void *args, lock_e lock)
{
    int idx;

    for (idx=0; idx<hmap->len; ++idx) {
        _hash_map_lock(hmap, idx, lock);
        rbt_trav(hmap->tree[idx], proc, args);
        _hash_map_unlock(hmap, idx, lock);
    }

    return 0;
}
