#include "search.h"
#include "srch_recver.h"

static srch_recver_t *srch_recver_get(srch_cntx_t *ctx);

/******************************************************************************
 **函数名称: srch_recver_routine
 **功    能: 运行接收线程
 **输入参数:
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *srch_recver_routine(void *_ctx)
{
    srch_recver_t *recver;
    srch_cntx_t *ctx = (srch_cntx_t *)_ctx;

    recver = srch_recver_get(ctx);
    if (NULL == recver)
    {
        return (void *)-1;
    }

    while (1)
    {
        NULL;
    }

    return NULL;
}

/******************************************************************************
 **函数名称: srch_recver_init
 **功    能: 初始化接收线程
 **输入参数:
 **     ctx: 全局信息
 **     recver: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int srch_recver_init(srch_cntx_t *ctx, srch_recver_t *recver)
{
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_destroy
 **功    能: 销毁接收线程
 **输入参数:
 **     recver: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int srch_recver_destroy(srch_recver_t *recver)
{
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_get
 **功    能: 获取Recver对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: Recver对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.26 #
 ******************************************************************************/
static srch_recver_t *srch_recver_get(srch_cntx_t *ctx)
{
    int tidx;

    tidx = thread_pool_get_tidx(ctx->recvers);

    return (srch_recver_t *)ctx->recvers->data + tidx;
}
