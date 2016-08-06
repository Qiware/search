/******************************************************************************
 ** Coypright(C) 2016-2026 Qiware technology Co., Ltd
 **
 ** 文件名: mem_ref.c
 ** 版本号: 1.0
 ** 描  述: 内存引用计数管理
 ** 作  者: # Qifeng.zou # 2016年06月27日 星期一 21时19分37秒 #
 ******************************************************************************/
#include "comm.h"
#include "atomic.h"
#include "rb_tree.h"
#include "mem_ref.h"

/* 内存引用项 */
typedef struct
{
    void *addr;                     // 内存地址
    uint32_t count;                 // 引用次数

    struct {
        void *pool;                 // 内存池
        mem_dealloc_cb_t dealloc;   // 释放回调
    };
} mem_ref_item_t;

/* 内存引用槽 */
typedef struct
{
    pthread_rwlock_t lock;          // 读写锁
    rbt_tree_t *tree;               // 内存引用表
} mem_ref_slot_t;

/* 全局对象 */
typedef struct
{
#define MEM_REF_SLOT_LEN (777)
    mem_ref_slot_t slot[MEM_REF_SLOT_LEN];
} mem_ref_cntx_t;

#define MEM_REF_IDX(addr) ((uint64_t)addr % MEM_REF_SLOT_LEN)

static mem_ref_cntx_t g_mem_ref_ctx; // 全局对象

static int mem_ref_add(void *addr, void *pool, mem_dealloc_cb_t dealloc);

/******************************************************************************
 **函数名称: mem_ref_init
 **功    能: 初始化内存引用计数表
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.29 14:45:15 #
 ******************************************************************************/
int mem_ref_init(void)
{
    int idx;
    mem_ref_slot_t *slot;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    memset(ctx, 0, sizeof(mem_ref_cntx_t));

    for (idx=0; idx<MEM_REF_SLOT_LEN; ++idx) {
        slot = &ctx->slot[idx];

        pthread_rwlock_init(&slot->lock, NULL);
        slot->tree = (rbt_tree_t *)rbt_creat(NULL, (cmp_cb_t)cmp_cb_ptr);
        if (NULL == slot->tree) {
            return -1;
        }
    }

    return 0;
}

/******************************************************************************
 **函数名称: mem_ref_alloc
 **功    能: 申请内存空间
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.04 00:33:34 #
 ******************************************************************************/
void *mem_ref_alloc(size_t size, void *pool,
        mem_alloc_cb_t alloc, mem_dealloc_cb_t dealloc)
{
    void *addr;

    addr = (void *)alloc(pool, size);
    if (NULL == addr) {
        return NULL;
    }

    mem_ref_add(addr, pool, dealloc);

    return addr;
}

/******************************************************************************
 **函数名称: mem_ref_add
 **功    能: 添加新的引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 引用次数
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.09.08 #
 ******************************************************************************/
static int mem_ref_add(void *addr, void *pool, mem_dealloc_cb_t dealloc)
{
    int cnt, idx;
    mem_ref_slot_t *slot;
    mem_ref_item_t *item;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    idx = MEM_REF_IDX(addr);
    slot = &ctx->slot[idx];

AGAIN:
    pthread_rwlock_rdlock(&slot->lock);

    /* > 查询引用 */
    item = (mem_ref_item_t *)rbt_query(slot->tree, (void *)&addr, sizeof(addr));
    if (NULL != item) {
        cnt = (int)atomic32_inc(&item->count);
        pthread_rwlock_unlock(&slot->lock);
        return cnt;
    }
    pthread_rwlock_unlock(&slot->lock);


    /* > 新增引用 */
    item = (mem_ref_item_t *)calloc(1, sizeof(mem_ref_item_t));
    if (NULL == item) {
        return -1;
    }

    item->addr = addr;
    cnt = ++item->count;
    item->pool = pool;
    item->dealloc = dealloc;

    pthread_rwlock_wrlock(&slot->lock);

    if (rbt_insert(slot->tree, (void *)&addr, sizeof(addr), item)) {
        pthread_rwlock_unlock(&slot->lock);
        free(item);
        goto AGAIN;
    }

    pthread_rwlock_unlock(&slot->lock);

    return cnt;
}

/******************************************************************************
 **函数名称: mem_ref_dealloc
 **功    能: 回收内存空间
 **输入参数: NONE
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.04 00:33:34 #
 ******************************************************************************/
void mem_ref_dealloc(void *pool, void *addr)
{
    mem_ref_decr(addr);
}

/******************************************************************************
 **函数名称: mem_ref_incr
 **功    能: 增加1次引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述:
 **注意事项: 内存addr必须由mem_ref_alloc进行分配.
 **作    者: # Qifeng.zou # 2016.07.06 #
 ******************************************************************************/
int mem_ref_incr(void *addr)
{
    int cnt, idx;
    mem_ref_slot_t *slot;
    mem_ref_item_t *item;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    idx = MEM_REF_IDX(addr);
    slot = &ctx->slot[idx];

    pthread_rwlock_rdlock(&slot->lock);

    item = (mem_ref_item_t *)rbt_query(slot->tree, (void *)&addr, sizeof(addr));
    if (NULL != item) {
        cnt = (int)atomic32_inc(&item->count);
        pthread_rwlock_unlock(&slot->lock);
        return cnt;
    }

    pthread_rwlock_unlock(&slot->lock);

    return -1; // 未创建结点
}

/******************************************************************************
 **函数名称: mem_ref_decr
 **功    能: 减少1次引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 内存引用次数
 **实现描述:
 **注意事项:
 **     1. 内存addr是通过系统调用分配的方可使用内存引用
 **     2. 如果引用计数减为0, 则释放该内存空间.
 **作    者: # Qifeng.zou # 2016.06.29 14:53:09 #
 ******************************************************************************/
int mem_ref_decr(void *addr)
{
    int cnt, idx;
    mem_ref_slot_t *slot;
    mem_ref_item_t *item, *temp;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    idx = MEM_REF_IDX(addr);
    slot = &ctx->slot[idx];

    pthread_rwlock_rdlock(&slot->lock);

    item = (mem_ref_item_t *)rbt_query(slot->tree, (void *)&addr, sizeof(addr));
    if (NULL == item) {
        pthread_rwlock_unlock(&slot->lock);
        return 0; // Didn't find
    }

    cnt = (int)atomic32_dec(&item->count);
    if (0 == cnt) {
        pthread_rwlock_unlock(&slot->lock);

        pthread_rwlock_wrlock(&slot->lock);
        item = (mem_ref_item_t *)rbt_query(slot->tree, (void *)&addr, sizeof(addr));
        if (NULL == item) {
            pthread_rwlock_unlock(&slot->lock);
            return 0; // Didn't find
        }

        if (0 == item->count) {
            rbt_delete(slot->tree, (void *)&addr, sizeof(addr), (void **)&temp);
            pthread_rwlock_unlock(&slot->lock);

            item->dealloc(item->pool, item->addr); // 释放被管理的内存
            free(item);
            return 0;
        }
        pthread_rwlock_unlock(&slot->lock);
        return 0;
    }

    pthread_rwlock_unlock(&slot->lock);
    return cnt;
}

/******************************************************************************
 **函数名称: mem_ref_check
 **功    能: 内存引用检测
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 引用次数
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.09.08 #
 ******************************************************************************/
int mem_ref_check(void *addr)
{
    int cnt, idx;
    mem_ref_slot_t *slot;
    mem_ref_item_t *item;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    idx = MEM_REF_IDX(addr);
    slot = &ctx->slot[idx];

    pthread_rwlock_rdlock(&slot->lock);

    /* > 查询引用 */
    item = (mem_ref_item_t *)rbt_query(slot->tree, (void *)&addr, sizeof(addr));
    if (NULL != item) {
        cnt = item->count;
        pthread_rwlock_unlock(&slot->lock);
        return cnt;
    }
    pthread_rwlock_unlock(&slot->lock);

    assert(0);

    return 0;
}


