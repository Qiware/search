#include "log.h"
#include "str.h"
#include "redis.h"
#include "common.h"

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
        const char *hname, const char *key, const char *value)
{
    redisReply *r;

    r = redisCommand(redis, "HSETNX %s %s %s", hname, key, value);
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
 **函数名称: redis_hlen
 **功    能: 获取哈希表长度
 **输入参数: 
 **     redis: Redis信息
 **     list: LIST名
 **输出参数:
 **返    回: 列表长度
 **实现描述: 
 **     LLEN:
 **         1. 返回哈希表的长度
 **         2. 当哈希表不存在时, 返回0
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.04.05 #
 ******************************************************************************/
int redis_hlen(redisContext *redis, const char *hname)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "HLEN %s", hname);
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
