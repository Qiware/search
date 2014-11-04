#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "redis.h"
#include "xd_str.h"
#include "common.h"

/******************************************************************************
 **函数名称: redis_ctx_init
 **功    能: 初始化Redis上下文
 **输入参数: 
 **     mcf: Master配置
 **     scf: Slave配置链表
 **输出参数:
 **返    回: Redis上下文
 **实现描述: 
 **     1. 申请内存空间
 **     2. 连接Master
 **     3. 依次连接Slave
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
redis_ctx_t *redis_ctx_init(const redis_conf_t *mcf, const list_t *scf)
{
    int idx;
    struct timeval tv;
    redis_ctx_t *ctx;
    list_node_t *node;
    redis_conf_t *conf;

    /* 1. 申请内存空间 */
    ctx = (redis_ctx_t *)calloc(1, sizeof(redis_ctx_t));
    if (NULL == ctx)
    {
        return NULL;
    }

    ctx->slave = (redisContext **)calloc(scf->num, sizeof(redisContext *));
    if (NULL == ctx->slave)
    {
        free(ctx);
        return NULL;
    }

    /* 2. 连接Master */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    ctx->master = redisConnectWithTimeout(mcf->ip, mcf->port, tv);
    if (NULL == ctx->master)
    {
        free(ctx->slave);
        free(ctx);
        return NULL;
    }

    /* 3. 依次连接Slave */
    ctx->slave_num = 0;
    node = scf->head;

    for (idx=0; idx<scf->num; ++idx, node = node->next)
    {
        ++ctx->slave_num;

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        conf = (redis_conf_t *)node->data;

        ctx->slave[idx] = redisConnectWithTimeout(conf->ip, conf->port, tv);
        if (ctx->slave[idx]->err)
        {
            redis_ctx_destroy(ctx);
            return NULL;
        }
    }

    return ctx;
}

/******************************************************************************
 **函数名称: redis_ctx_destroy
 **功    能: 销毁Redis上下文
 **输入参数: 
 **     ctx: Redis上下文
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
void redis_ctx_destroy(redis_ctx_t *ctx)
{
    int idx;

    for (idx=0; idx<ctx->slave_num; ++idx)
    {
        if (NULL != ctx->slave[idx])
        {
            redisFree(ctx->slave[idx]);
            ctx->slave[idx] = NULL;
        }
    }

    redisFree(ctx->master);
    free(ctx->slave);
    free(ctx);
}

/******************************************************************************
 **函数名称: redis_hsetnx
 **功    能: 设置HASH表中指定KEY的值(前提: 当指定KEY不存在时才更新)
 **输入参数: 
 **     ctx: Redis信息
 **     hash: HASH表名
 **     key: 指定KEY
 **     value: 设置值
 **输出参数:
 **返    回: true:更新成功 false:更新失败(已存在)
 **实现描述: 
 **     HSETNX:
 **         1. 表示新的Value被设置了新值
 **         0. 表示Key已经存在,该命令没有进行任何操作.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.26 #
 ******************************************************************************/
bool redis_hsetnx(redisContext *ctx,
        const char *hash, const char *key, const char *value)
{
    redisReply *r;

    r = redisCommand(ctx, "HSETNX %s %s %s", hash, key, value);
    if (REDIS_REPLY_INTEGER != r->type)
    {
        freeReplyObject(r);
        return false;
    }

    if (1 == r->integer)
    {
        freeReplyObject(r);
        return true;
    }

    freeReplyObject(r);
    return false;
}
