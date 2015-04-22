#include "log.h"
#include "str.h"
#include "comm.h"
#include "redis.h"

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
