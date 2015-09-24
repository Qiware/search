#include "log.h"
#include "str.h"
#include "comm.h"
#include "redis.h"

/******************************************************************************
 **函数名称: redis_llen
 **功    能: 获取列表长度
 **输入参数: 
 **     redis: Redis信息
 **     ln: 列表名
 **输出参数:
 **返    回: 列表长度
 **实现描述: 
 **     LLEN:
 **         1) 返回列表的长度
 **         2) 当列表不存在时, 返回0
 **         3) 当ln不是列表时, 返回错误.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.03.08 #
 ******************************************************************************/
int redis_llen(redisContext *redis, const char *ln)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "LLEN %s", ln);
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

/******************************************************************************
 **函数名称: redis_lpush
 **功    能: 将一个或多个value插入到链表头(最左边)
 **输入参数: 
 **     redis: Redis信息
 **     ln: 列表名
 **     value: 将被插入到链表的数值
 **输出参数:
 **返    回: 操作完成后, 链表长度
 **实现描述: 
 **     LPUSH:
 **     1) 时间复杂度: O(1)
 **     2) 当ln不存在时, 该命令首先会新建一个与ln关联的空链表, 再将数据插入到链表的头部;
 **     3) 当ln关联的不是List类型, 将返回错误信息;
 **     4) 当有多个value值, 那么各个value值按从左到右的顺序依次插入到表头: 比如说,
 **        对空列表mylist执行命令LPUSH mylist a b c, 列表的值将是 c b a, 这等同于
 **        原子性地执行LPUSH mylist a 、 LPUSH mylist b 和 LPUSH mylist c 三个命令.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
int redis_lpush(redisContext *redis, const char *ln, const char *values)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "LPUSH %s %s", ln, values);
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

/******************************************************************************
 **函数名称: redis_rpush
 **功    能: 将一个或多个value插入到链表头(最右边)
 **输入参数: 
 **     redis: Redis信息
 **     ln: 列表名
 **     value: 将被插入到链表的数值
 **输出参数:
 **返    回: 操作完成后, 链表长度
 **实现描述: 
 **     RPUSH:
 **     1) 时间复杂度: O(1)
 **     2) 当ln不存在时, 该命令首先会新建一个与ln关联的空链表, 再将数据插入到链表的头部;
 **     3) 当ln关联的不是List类型, 将返回错误信息;
 **     4) 当有多个value值, 那么各个value值按从左到右的顺序依次插入到表头: 比如说,
 **        对空列表mylist执行命令RPUSH mylist a b c, 列表的值将是 c b a, 这等同于
 **        原子性地执行RPUSH mylist a 、 RPUSH mylist b 和 RPUSH mylist c 三个命令.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
int redis_rpush(redisContext *redis, const char *ln, const char *values)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "RPUSH %s %s", ln, values);
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

/******************************************************************************
 **函数名称: redis_lpushx
 **功    能: 将一个value插入到链表头(最左边)
 **输入参数: 
 **     redis: Redis信息
 **     ln: 列表名
 **     value: 将被插入到链表的数值
 **输出参数:
 **返    回: 操作完成后, 链表长度
 **实现描述: 
 **     LPUSHX:
 **     1) 时间复杂度: O(1)
 **     2) 当前仅当ln存在时, 才将value插入到链表头;
 **     3) 当ln不存在时, 该命令将不起任何作用.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
int redis_lpushx(redisContext *redis, const char *ln, const char *value)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "LPUSHX %s %s", ln, value);
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

/******************************************************************************
 **函数名称: redis_rpushx
 **功    能: 将一个value插入到链表头(最右边)
 **输入参数: 
 **     redis: Redis信息
 **     ln: 列表名
 **     value: 将被插入到链表的数值
 **输出参数:
 **返    回: 操作完成后, 链表长度
 **实现描述: 
 **     RPUSHX:
 **     1) 时间复杂度: O(1)
 **     2) 当前仅当ln存在时, 才将value插入到链表头;
 **     3) 当ln不存在时, 该命令将不起任何作用.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
int redis_rpushx(redisContext *redis, const char *ln, const char *value)
{
    int len;
    redisReply *r;

    r = redisCommand(redis, "LPUSHX %s %s", ln, value);
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
