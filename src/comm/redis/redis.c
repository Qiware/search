#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>

#include "log.h"
#include "str.h"
#include "redis.h"
#include "common.h"

/******************************************************************************
 **函数名称: redis_clst_init
 **功    能: 初始化Redis集群
 **输入参数: 
 **     master_cf: Master配置
 **     slave_cf: Slave配置数组
 **     slave_num: Slave配置数组长度
 **输出参数:
 **返    回: Redis上下文
 **实现描述: 
 **     1. 申请内存空间
 **     2. 连接Master
 **     3. 依次连接Slave
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
redis_clst_t *redis_clst_init(const redis_conf_t *master_cf, const redis_conf_t *slave_cf, int slave_num)
{
    int idx;
    struct timeval tv;
    redis_clst_t *clst;
    redisContext *redis;
    const redis_conf_t *conf;

    /* 1. 申请内存空间 */
    clst = (redis_clst_t *)calloc(1, sizeof(redis_clst_t));
    if (NULL == clst)
    {
        return NULL;
    }

    clst->redis = (redisContext **)calloc(1, slave_num + 1);
    if (NULL == clst->redis)
    {
        free(clst);
        return NULL;
    }

    /* 2. 连接Master */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    clst->redis[0] = redisConnectWithTimeout(master_cf->ip, master_cf->port, tv);
    if (clst->redis[0]->err)
    {
        free(clst->redis);
        free(clst);
        return NULL;
    }

    ++clst->num;

    /* 3. 依次连接Slave */
    clst->num = 0;
    for (idx=0; idx<slave_num; ++idx)
    {
        redis = clst->redis[1] + idx;
        ++clst->num;

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        conf = slave_cf + idx;

        redis = redisConnectWithTimeout(conf->ip, conf->port, tv);
        if (redis->err)
        {
            redis_clst_destroy(clst);
            return NULL;
        }
    }

    return clst;
}

/******************************************************************************
 **函数名称: redis_clst_destroy
 **功    能: 销毁Redis集群
 **输入参数: 
 **     clst: Redis集群
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     1. 销毁副本上下文
 **     1. 销毁主机上下文
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
void redis_clst_destroy(redis_clst_t *clst)
{
    int idx;

    for (idx=0; idx<clst->num; ++idx)
    {
        if (NULL != clst->redis[idx])
        {
            redisFree(clst->redis[idx]);
            clst->redis[idx] = NULL;
        }
    }

    free(clst->redis);
    free(clst);
}

/******************************************************************************
 **函数名称: redis_hsetnx
 **功    能: 设置HASH表中指定KEY的值(前提: 当指定KEY不存在时才更新)
 **输入参数: 
 **     redis: Redis信息
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
bool redis_hsetnx(redisContext *redis,
        const char *hash, const char *key, const char *value)
{
    redisReply *r;

    r = redisCommand(redis, "HSETNX %s %s %s", hash, key, value);
    if (NULL == r
        || REDIS_REPLY_INTEGER != r->type)
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

/******************************************************************************
 **函数名称: redis_llen
 **功    能: 获取列表长度
 **输入参数: 
 **     redis: Redis信息
 **     list: LIST名
 **输出参数:
 **返    回: 列表长度
 **实现描述: 
 **     LLEN:
 **         1. 返回列表的长度
 **         2. 当列表不存在时, 返回0
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.08 #
 ******************************************************************************/
int redis_llen(redisContext *redis, const char *list)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "LLEN %s", list);
    if (NULL == r
        || REDIS_REPLY_INTEGER != r->type)
    {
        freeReplyObject(r);
        return 0;
    }

    len = r->integer;

    freeReplyObject(r);
    return len;
}
