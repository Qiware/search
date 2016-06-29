/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: mem_ref.c
 ** 版本号: 1.0
 ** 描  述: 内存引用技术管理
 ** 作  者: # Qifeng.zou # 2016年06月27日 星期一 21时19分37秒 #
 ******************************************************************************/
#include "comm.h"
#include "rb_tree.h"

typedef struct
{
    void *addr;     // 内存地址
    uint64_t count; // 引用次数
} mem_ref_item_t;

typedef struct
{
    rbt_tree_t *tab;// 内存引用表
} mem_ref_cntx_t;

static mem_ref_cntx_t g_mem_ref_ctx;

static uint64_t mem_ref_key_cb(const void *key, size_t len) { return (uint64_t)key; }
static inline int mem_ref_cmp_cb(const void *key, const void *data) { return 0; } 

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
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    memset(ctx, 0, sizeof(mem_ref_cntx_t));

    ctx->tab = (rbt_tree_t *)rbt_creat(NULL, 
            (key_cb_t)mem_ref_key_cb, (cmp_cb_t)mem_ref_cmp_cb);

    return (NULL == ctx->tab)? -1 : 0;
}

/******************************************************************************
 **函数名称: mem_ref_incr
 **功    能: 增加1次引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 内存池对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.08 #
 ******************************************************************************/
int mem_ref_incr(void *addr)
{
    mem_ref_item_t *item;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    item = (mem_ref_item_t *)rbt_query(ctx->tab, addr, sizeof(addr));
    if (NULL != item) {
        ++item->count;
        return 0;
    }

    item = (mem_ref_item_t *)calloc(1, sizeof(mem_ref_item_t));
    if (NULL == item) {
        return -1;
    }

    item->addr = addr;
    ++item->count;

    if (rbt_insert(ctx->tab, addr, sizeof(addr), item)) {
        free(item);
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: mem_ref_sub
 **功    能: 减少1次引用
 **输入参数:
 **     addr: 内存地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.06.29 14:53:09 #
 ******************************************************************************/
int mem_ref_sub(void *addr)
{
    mem_ref_item_t *item;
    mem_ref_cntx_t *ctx = &g_mem_ref_ctx;

    item = (mem_ref_item_t *)rbt_query(ctx->tab, addr, sizeof(addr));
    if (NULL == item) {
        return 0; // Didn't find
    }

    if (0 == --item->count) {
        rbt_delete(ctx->tab, addr, sizeof(addr), (void **)&item);
        free(item->addr);
        free(item);
        return 0;
    }

    return 0;
}
