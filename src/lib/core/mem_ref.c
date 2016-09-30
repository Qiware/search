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
#include "mem_ref.h"
#include "hash_tab.h"

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

/* 内存应用管理表 */
hash_tab_t *g_mem_ref_tab;

static int mem_ref_add(void *addr, void *pool, mem_dealloc_cb_t dealloc);

/* 哈希回调函数 */
static uint64_t mem_ref_hash_cb(const mem_ref_item_t *item)
{
    return (uint64_t)item->addr;
}

/* 比较回调函数 */
static int mem_ref_cmp_cb(const mem_ref_item_t *item1, const mem_ref_item_t *item2)
{
    return ((uint64_t)item1->addr - (uint64_t)item2->addr);
}

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
    g_mem_ref_tab = hash_tab_creat(333,
            (hash_cb_t)mem_ref_hash_cb,
            (cmp_cb_t)mem_ref_cmp_cb, NULL);

    return (NULL == g_mem_ref_tab)? -1 : 0;
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
    int cnt;
    mem_ref_item_t *item, key;

AGAIN:
    /* > 查询引用 */
    key.addr = addr;

    item = hash_tab_query(g_mem_ref_tab, (void *)&key, RDLOCK);
    if (NULL != item) {
        cnt = (int)atomic32_inc(&item->count);
        hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);
        return cnt;
    }
    hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);

    /* > 新增引用 */
    item = (mem_ref_item_t *)calloc(1, sizeof(mem_ref_item_t));
    if (NULL == item) {
        return -1;
    }

    item->addr = addr;
    cnt = ++item->count;
    item->pool = pool;
    item->dealloc = dealloc;

    if (hash_tab_insert(g_mem_ref_tab, item, WRLOCK)) {
        free(item);
        goto AGAIN;
    }

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
    int cnt;
    mem_ref_item_t *item, key;

    key.addr = addr;

    item = hash_tab_query(g_mem_ref_tab, (void *)&key, RDLOCK);
    if (NULL != item) {
        cnt = (int)atomic32_inc(&item->count);
        hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);
        return cnt;
    }
    hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);

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
    int cnt;
    mem_ref_item_t *item, key;

    key.addr = addr;

    item = hash_tab_query(g_mem_ref_tab, (void *)&key, RDLOCK);
    if (NULL == item) {
        return 0; // Didn't find
    }

    cnt = (int)atomic32_dec(&item->count);

    hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);

    if (0 == cnt) {
        item = hash_tab_query(g_mem_ref_tab, (void *)&key, WRLOCK);
        if (NULL == item) {
            return 0; // Didn't find
        }
        else if (0 == item->count) {
            hash_tab_delete(g_mem_ref_tab, (void *)&key, NONLOCK);
            hash_tab_unlock(g_mem_ref_tab, &key, WRLOCK);

            item->dealloc(item->pool, item->addr); // 释放被管理的内存
            free(item);
            return 0;
        }
        hash_tab_unlock(g_mem_ref_tab, &key, WRLOCK);
        return 0;
    }
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
    int cnt;
    mem_ref_item_t *item, key;

    /* > 查询引用 */
    key.addr = addr;

    item = (mem_ref_item_t *)hash_tab_query(g_mem_ref_tab, (void *)&key, RDLOCK);
    if (NULL != item) {
        cnt = item->count;
        hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);
        return cnt;
    }
    hash_tab_unlock(g_mem_ref_tab, &key, RDLOCK);

    return 0;
}


