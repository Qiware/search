#if !defined(__REDIS_H__)
#define __REDIS_H__

#include "comm.h"
#include "list.h"
#include <hiredis/hiredis.h>

#define REDIS_CONN_TMOUT_SEC    (30)    /* 连接超时:秒 */
#define REDIS_CONN_TMOUT_USEC   (0)     /* 连接超时:微妙 */

/* Redis配置 */
typedef struct
{
    char ip[IP_ADDR_MAX_LEN];       /* IP */
    int port;                       /* 端口号 */
} redis_conf_t;

/* Redis集群对象 */
typedef struct
{
    int num;                        /* REDIS对象数 */
#define REDIS_MASTER_IDX (0)        /* MASTER索引 */
    redisContext **redis;           /* REDIS对象(注: [0]为Master [1~num-1]为Slave */
} redis_clst_t;

redis_clst_t *redis_clst_init(const redis_conf_t *conf, int num);
void redis_clst_destroy(redis_clst_t *clst);

bool redis_hsetnx(redisContext *ctx, const char *hname, const char *key, const char *value);
int redis_hlen(redisContext *ctx, const char *hname);

/******************************************************************************
 **函数名称: redis_lpop
 **功    能: 移除并返回链表头元素(最左边)
 **输入参数: 
 **     ctx: Redis信息
 **     ln: 链表名
 **输出参数:
 **返    回: 链表头元素
 **实现描述: 
 **     LPOP:
 **     1) 时间复杂度: O(1)
 **     2) 当链表ln不存在时, 将返回NIL;
 **     3) 当ln不是List类型时, 将返回错误信息.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#define redis_lpop(ctx, ln) redisCommand(ctx, "LPOP %s", ln)

/******************************************************************************
 **函数名称: redis_rpop
 **功    能: 移除并返回链表尾元素(最右边)
 **输入参数: 
 **     ctx: Redis信息
 **     ln: 链表名
 **输出参数:
 **返    回: 链表尾元素
 **实现描述: 
 **     LPOP:
 **     1) 时间复杂度: O(1)
 **     2) 当链表ln不存在时, 将返回NIL;
 **     3) 当ln不是List类型时, 将返回错误信息.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
#define redis_rpop(ctx, ln) redisCommand(ctx, "RPOP %s", ln)

int redis_llen(redisContext *ctx, const char *ln);
int redis_lpush(redisContext *ctx, const char *ln, const char *values);
int redis_rpush(redisContext *ctx, const char *ln, const char *values);
int redis_lpushx(redisContext *ctx, const char *ln, const char *value);
int redis_rpushx(redisContext *ctx, const char *ln, const char *value);

/******************************************************************************
 **函数名称: redis_rpop_lpush
 **功    能: 该命令将原子性的执行以下两条命令:
 **         1. 将列表source中的最后一个元素(尾元素)弹出,并返回给客户端.
 **         2. 将source弹出的元素插入到列表destination,作为destination列表的的头元素.
 **输入参数: 
 **     ctx: Redis信息
 **     sln: 原列表名
 **     dln: 目的列表名
 **输出参数:
 **返    回: 原链表尾元素
 **实现描述: 
 **     RPOPLPUSH:
 **     1) 时间复杂度: O(1)
 **     2) 如果source不存在,值NULL被返回,并且不执行其他动作;
 **     3) 如果source和destination相同,则列表中的表尾元素被移动到表头，并返回该
 **        元素,可以把这种特殊情况视作列表的旋转(rotation)操作.
 **     4) 举个例子,你有两个列表source和destination, source列表有元素 a, b, c,
 **        destination列表有元素 x, y, z, 执行RPOPLPUSH source destination之后,
 **        source列表包含元素 a, b, destination列表包含元素 c, x, y, z, 并且元
 **        素c会被返回给客户端.
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.09.23 #
 ******************************************************************************/
#define redis_rpop_lpush(ctx, sln, dln) redisCommand(ctx, "RPOPLPUSH %s %s", sln, dln)

#endif /*__REDIS_H__*/
