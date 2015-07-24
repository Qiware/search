#include "log.h"
#include "str.h"
#include "comm.h"
#include "redis.h"

/******************************************************************************
 **函数名称: redis_clst_init
 **功    能: 初始化Redis集群
 **输入参数: 
 **     conf: Redis配置数组
 **     num: 数组长度
 **输出参数:
 **返    回: Redis上下文
 **实现描述: 
 **     1. 申请内存空间
 **     2. 连接Master
 **     3. 依次连接Slave
 **注意事项: 
 **     conf[0]: 表示的是Master配置
 **     conf[1~num-1]: 表示的是副本配置
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
redis_clst_t *redis_clst_init(const redis_conf_t *conf, int num)
{
    int idx;
    struct timeval tv;
    redis_clst_t *clst;

    /* 1. 申请内存空间 */
    clst = (redis_clst_t *)calloc(1, sizeof(redis_clst_t));
    if (NULL == clst)
    {
        return NULL;
    }

    clst->redis = (redisContext **)calloc(num, sizeof(redisContext *));
    if (NULL == clst->redis)
    {
        free(clst);
        return NULL;
    }

    /* 2. 依次连接REDIS */
    clst->num = num;
    for (idx=0; idx<num; ++idx)
    {
        tv.tv_sec = REDIS_CONN_TMOUT_SEC;
        tv.tv_usec = REDIS_CONN_TMOUT_USEC;

        clst->redis[idx] = redisConnectWithTimeout(conf[idx].ip, conf[idx].port, tv);
        if (clst->redis[idx]->err)
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
 **实现描述: 依次销毁集群各成员上下文
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
