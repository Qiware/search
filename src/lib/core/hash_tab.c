/******************************************************************************
 ** Copyright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: hash_tab.c
 ** 版本号: 1.0
 ** 描  述: 哈希表模块
 **         1. 使用哈希数组分解锁的压力
 **         2. 使用红黑树解决数据查找的性能问题
 ** 作  者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
#include "rb_tree.h"
#include "hash_tab.h"

static void _hash_tab_lock(hash_tab_t *htab, int idx, lock_e lock)
{
    if (WRLOCK == lock) {
        pthread_rwlock_wrlock(&htab->lock[idx]);
    }
    else if (RDLOCK == lock) {
        pthread_rwlock_rdlock(&htab->lock[idx]);
    }
}

static void _hash_tab_unlock(hash_tab_t *htab, int idx, lock_e lock)
{
    if ((WRLOCK == lock) || (RDLOCK == lock)) {
        pthread_rwlock_unlock(&htab->lock[idx]);
    }
}

/******************************************************************************
 **函数名称: hash_tab_unlock
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
void hash_tab_unlock(hash_tab_t *htab, void *key, lock_e lock)
{
    unsigned int idx;

    idx = htab->hash(key) % htab->len;

    if ((WRLOCK == lock) || (RDLOCK == lock)) {
        pthread_rwlock_unlock(&htab->lock[idx]);
    }
}

/******************************************************************************
 **函数名称: hash_tab_creat
 **功    能: 创建哈希表
 **输入参数:
 **     len: 哈希表长度
 **     key: 生成KEY的函数
 **     cmp: 数据比较函数
 **     opt: 其他选项
 **输出参数: NONE
 **返    回: 哈希数组地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
hash_tab_t *hash_tab_creat(int len, hash_cb_t hash, cmp_cb_t cmp, hash_tab_opt_t *opt)
{
    int idx;
    hash_tab_t *htab;
    rbt_opt_t rbt_opt;
    hash_tab_opt_t hmap_opt;

    if (NULL == opt) {
        memset(&hmap_opt, 0, sizeof(hmap_opt));

        hmap_opt.pool = (void *)NULL;
        hmap_opt.alloc = (mem_alloc_cb_t)mem_alloc;
        hmap_opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

        opt = &hmap_opt;
    }

    /* > 创建哈希数组 */
    htab = (hash_tab_t *)opt->alloc(opt->pool, sizeof(hash_tab_t));
    if (NULL == htab) {
        return NULL;
    }

    htab->total = 0;
    htab->len = len;
    htab->cmp = cmp;
    htab->hash = hash;
    htab->pool = (void *)opt->pool;
    htab->alloc = (mem_alloc_cb_t)opt->alloc;
    htab->dealloc = (mem_dealloc_cb_t)opt->dealloc;

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

        htab->tree[idx] = rbt_creat(&rbt_opt, cmp);
        if (NULL == htab->tree[idx]) {
            hash_tab_destroy(htab, mem_dummy_dealloc, NULL);
            return NULL;
        }
    }

    return htab;
}

/******************************************************************************
 **函数名称: hash_tab_insert
 **功    能: 插入哈希成员
 **输入参数:
 **     htab: 哈希数组
 **     data: 需要插入的数据
 **     lock: 锁操作
 **输出参数:
 **     data: 数据
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_tab_insert(hash_tab_t *htab, void *data, lock_e lock)
{
    int ret;
    unsigned int idx;

    idx = htab->hash(data) % htab->len;

    _hash_tab_lock(htab, idx, lock);
    ret = rbt_insert(htab->tree[idx], data);
    if (0 == ret) {
        ++htab->total;
    }
    _hash_tab_unlock(htab, idx, lock);

    return ret;
}

/******************************************************************************
 **函数名称: hash_tab_query
 **功    能: 查找哈希成员
 **输入参数:
 **     htab: 哈希数组
 **     key: 主键
 **     lock: 锁操作
 **输出参数:
 **返    回: 查询的数据
 **实现描述:
 **注意事项:
 **     1. 当lock为NONLOCK时, 用完查询的数据后, 无需调用hash_tab_unlock()释放锁.
 **     2. 当lock为WRLOCK/RDLOCK时, 用完查询的数据后, 需要调用hash_tab_unlock()释放锁.
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
void *hash_tab_query(hash_tab_t *htab, void *key, lock_e lock)
{
    void *data;
    unsigned int idx;

    idx = htab->hash(key) % htab->len;

    _hash_tab_lock(htab, idx, lock);
    data = rbt_query((void *)htab->tree[idx], key);
    if (NULL == data) {
        _hash_tab_unlock(htab, idx, lock);
        return NULL; /* 未找到 */
    }

    return data;
}

/******************************************************************************
 **函数名称: hash_tab_delete
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
void *hash_tab_delete(hash_tab_t *htab, void *key, lock_e lock)
{
    void *data;
    unsigned int idx;

    idx = htab->hash(key) % htab->len;

    _hash_tab_lock(htab, idx, lock);
    rbt_delete(htab->tree[idx], key, &data);
    if (NULL != data) {
        --htab->total;
    }
    _hash_tab_unlock(htab, idx, lock);

    return data;
}

/******************************************************************************
 **函数名称: hash_tab_destroy
 **功    能: 销毁哈希数组
 **输入参数:
 **     htab: 哈希数组
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.10.22 #
 ******************************************************************************/
int hash_tab_destroy(hash_tab_t *htab, mem_dealloc_cb_t dealloc, void *args)
{
    int idx;

    for (idx=0; idx<htab->len; ++idx) {
        pthread_rwlock_wrlock(&htab->lock[idx]);
        if (NULL != htab->tree[idx]) {
            rbt_destroy(htab->tree[idx], dealloc, args);
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
 **函数名称: hash_tab_trav
 **功    能: 遍历哈希数组
 **输入参数:
 **     htab: 哈希数组
 **     proc: 回调函数
 **     args: 附加参数
 **     lock: 加锁方式
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.12.24 #
 ******************************************************************************/
int hash_tab_trav(hash_tab_t *htab, trav_cb_t proc, void *args, lock_e lock)
{
    int idx;

    for (idx=0; idx<htab->len; ++idx) {
        _hash_tab_lock(htab, idx, lock);
        rbt_trav(htab->tree[idx], proc, args);
        _hash_tab_unlock(htab, idx, lock);
    }

    return 0;
}
