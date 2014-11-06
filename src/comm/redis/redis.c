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
 **函数名称: redis_cluster_init
 **功    能: 初始化Redis集群
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
redis_cluster_t *redis_cluster_init(const redis_conf_t *mcf, const list_t *scf)
{
    int idx;
    struct timeval tv;
    redis_cluster_t *cluster;
    list_node_t *node;
    redis_conf_t *conf;

    /* 1. 申请内存空间 */
    cluster = (redis_cluster_t *)calloc(1, sizeof(redis_cluster_t));
    if (NULL == cluster)
    {
        return NULL;
    }

    cluster->slave = (redisContext **)calloc(scf->num, sizeof(redisContext *));
    if (NULL == cluster->slave)
    {
        free(cluster);
        return NULL;
    }

    /* 2. 连接Master */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    cluster->master = redisConnectWithTimeout(mcf->ip, mcf->port, tv);
    if (NULL == cluster->master)
    {
        free(cluster->slave);
        free(cluster);
        return NULL;
    }

    /* 3. 依次连接Slave */
    cluster->slave_num = 0;
    node = scf->head;

    for (idx=0; idx<scf->num; ++idx, node = node->next)
    {
        ++cluster->slave_num;

        tv.tv_sec = 30;
        tv.tv_usec = 0;

        conf = (redis_conf_t *)node->data;

        cluster->slave[idx] = redisConnectWithTimeout(conf->ip, conf->port, tv);
        if (cluster->slave[idx]->err)
        {
            redis_cluster_destroy(cluster);
            return NULL;
        }
    }

    return cluster;
}

/******************************************************************************
 **函数名称: redis_cluster_destroy
 **功    能: 销毁Redis集群
 **输入参数: 
 **     cluster: Redis集群
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **     1. 销毁副本上下文
 **     1. 销毁主机上下文
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
void redis_cluster_destroy(redis_cluster_t *cluster)
{
    int idx;

    for (idx=0; idx<cluster->slave_num; ++idx)
    {
        if (NULL != cluster->slave[idx])
        {
            redisFree(cluster->slave[idx]);
            cluster->slave[idx] = NULL;
        }
    }

    redisFree(cluster->master);
    free(cluster->slave);
    free(cluster);
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
