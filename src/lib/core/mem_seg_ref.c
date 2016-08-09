/******************************************************************************
 ** Coypright(C) 2016-2026 Qiware technology Co., Ltd
 **
 ** 文件名: mem_seg_ref.c
 ** 版本号: 1.0
 ** 描  述: 内存分段引用计数管理
 ** 注意事项: 内存分段引用计数管理使用在单进程的环境更合适. 由于加锁的范围较大, 
 **           在多线程情况下, 可能对性能造成较大影响.
 ** 作  者: # Qifeng.zou # 2016年06月27日 星期一 21时19分37秒 #
 ******************************************************************************/
#include "comm.h"
#include "atomic.h"
#include "rb_tree.h"
#include "mem_seg_ref.h"

/* 内存引用项 */
typedef struct
{
    void *addr;                     // 内存地址
    size_t size;                    // 内存长度
    uint32_t count;                 // 引用次数

    struct {
        void *pool;                 // 内存池
        mem_alloc_cb_t alloc;       // 申请回调
        mem_dealloc_cb_t dealloc;   // 释放回调
    };
} mem_seg_ref_item_t;

/* 全局对象 */
typedef struct
{
    pthread_rwlock_t lock;
    rbt_tree_t *tree;    // 内存引用表
} mem_seg_ref_cntx_t;

static mem_seg_ref_cntx_t g_msr_ctx; // 全局对象

static int mem_seg_ref_add(void *addr, size_t size, void *pool, mem_alloc_cb_t alloc, mem_dealloc_cb_t dealloc);

/******************************************************************************
 **函数名称: mem_seg_ref_cmp
 **功    能: 查找匹配
 **输入参数: 
 **输出参数: NONE
 **返    回: 0:相等/<0:小于/>0:大于
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.26 22:51:17 #
 ******************************************************************************/
static int mem_seg_ref_cmp(const mem_seg_ref_item_t *item1, const mem_seg_ref_item_t *item2)
{
    if (item1->addr < item2->addr) {
        return -1; // 小于
    }
    else if (item1->addr > (item2->addr + item2->size - 1)) {
        return 1; // 大于
    }

    return 0; // 等于
}

/******************************************************************************
 **函数名称: mem_seg_ref_init
 **功    能: 初始化内存引用计数表
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.29 14:45:15 #
 ******************************************************************************/
int mem_seg_ref_init(void)
{
    mem_seg_ref_cntx_t *ctx = &g_msr_ctx;

    memset(ctx, 0, sizeof(mem_seg_ref_cntx_t));

    pthread_rwlock_init(&ctx->lock, NULL);

    ctx->tree = (rbt_tree_t *)rbt_creat(NULL, (cmp_cb_t)mem_seg_ref_cmp);
    if (NULL == ctx->tree) {
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: mem_seg_ref_alloc
 **功    能: 申请内存空间
 **输入参数: NONE
 **输出参数: NONE
 **返    回: 内存地址
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.04 00:33:34 #
 ******************************************************************************/
void *mem_seg_ref_alloc(size_t size, void *pool,
        mem_alloc_cb_t alloc, mem_dealloc_cb_t dealloc)
{
    void *addr;

    addr = (void *)alloc(pool, size);
    if (NULL == addr) {
        return NULL;
    }

    mem_seg_ref_add(addr, size, pool, alloc, dealloc);

    return addr;
}

/******************************************************************************
 **函数名称: mem_seg_ref_add
 **功    能: 添加新的引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述:
 **注意事项: 内存addr是通过系统调用分配的方可使用内存引用
 **作    者: # Qifeng.zou # 2016.09.08 #
 ******************************************************************************/
static int mem_seg_ref_add(void *addr, size_t size,
        void *pool, mem_alloc_cb_t alloc, mem_dealloc_cb_t dealloc)
{
    int cnt;
    mem_seg_ref_item_t *item, key;
    mem_seg_ref_cntx_t *ctx = &g_msr_ctx;

    key.addr = addr;

AGAIN:
    pthread_rwlock_rdlock(&ctx->lock);

    /* > 查询引用 */
    item = (mem_seg_ref_item_t *)rbt_query(ctx->tree, (void *)&key);
    if (NULL != item) {
        if (item->size != size) {
            pthread_rwlock_unlock(&ctx->lock);
            return -1;
        }
        cnt = (int)atomic32_inc(&item->count);
        pthread_rwlock_unlock(&ctx->lock);
        return cnt;
    }
    pthread_rwlock_unlock(&ctx->lock);


    /* > 新增引用 */
    item = (mem_seg_ref_item_t *)calloc(1, sizeof(mem_seg_ref_item_t));
    if (NULL == item) {
        return -1;
    }

    item->addr = addr;
    item->size = size;
    cnt = ++item->count;
    item->pool = pool;
    item->alloc = alloc;
    item->dealloc = dealloc;

    pthread_rwlock_wrlock(&ctx->lock);

    if (rbt_insert(ctx->tree, item)) {
        pthread_rwlock_unlock(&ctx->lock);
        free(item);
        goto AGAIN;
    }

    pthread_rwlock_unlock(&ctx->lock);

    return cnt;
}

/******************************************************************************
 **函数名称: mem_seg_ref_dealloc
 **功    能: 回收内存空间
 **输入参数: NONE
 **输出参数: NONE
 **返    回: VOID
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.07.04 00:33:34 #
 ******************************************************************************/
void mem_seg_ref_dealloc(void *pool, void *addr)
{
    mem_seg_ref_decr(addr);
}

/******************************************************************************
 **函数名称: mem_seg_ref_incr
 **功    能: 增加1次引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述:
 **注意事项: 内存addr必须由mem_seg_ref_alloc进行分配.
 **作    者: # Qifeng.zou # 2016.07.06 #
 ******************************************************************************/
int mem_seg_ref_incr(void *addr)
{
    int cnt;
    mem_seg_ref_item_t *item, key;
    mem_seg_ref_cntx_t *ctx = &g_msr_ctx;

    key.addr = addr;

    pthread_rwlock_rdlock(&ctx->lock);

    item = (mem_seg_ref_item_t *)rbt_query(ctx->tree, (void *)&key);
    if (NULL != item) {
        cnt = (int)atomic32_inc(&item->count);
        pthread_rwlock_unlock(&ctx->lock);
        return cnt;
    }

    pthread_rwlock_unlock(&ctx->lock);

    return -1; // 未创建结点
}

/******************************************************************************
 **函数名称: mem_seg_ref_decr
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
int mem_seg_ref_decr(void *addr)
{
    int cnt;
    mem_seg_ref_item_t *item, *temp, key;
    mem_seg_ref_cntx_t *ctx = &g_msr_ctx;

    key.addr = addr;

    pthread_rwlock_rdlock(&ctx->lock);

    item = (mem_seg_ref_item_t *)rbt_query(ctx->tree, (void *)&key);
    if (NULL == item) {
        pthread_rwlock_unlock(&ctx->lock);
        return 0; // Didn't find
    }

    cnt = (int)atomic32_dec(&item->count);
    if (0 == cnt) {
        pthread_rwlock_unlock(&ctx->lock);

        pthread_rwlock_wrlock(&ctx->lock);
        if (0 == item->count) {
            rbt_delete(ctx->tree, (void *)&key, (void **)&temp);
            pthread_rwlock_unlock(&ctx->lock);

            item->dealloc(item->pool, item->addr); // 释放被管理的内存
            free(item);
            return 0;
        }
        pthread_rwlock_unlock(&ctx->lock);
        return 0;
    }

    pthread_rwlock_unlock(&ctx->lock);
    return cnt;
}
