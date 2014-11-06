#if !defined(__REDIS_H__)
#define __REDIS_H__

#include <hiredis/hiredis.h>

#include "list.h"
#include "common.h"

/* Redis配置 */
typedef struct
{
    char ip[IP_ADDR_MAX_LEN];       /* IP */
    int port;                       /* 端口号 */
} redis_conf_t;

/* Redis集群对象 */
typedef struct
{
    redisContext *master;           /* 主对象 */
    int slave_num;                  /* 副本数 */
    redisContext **slave;           /* 副本对象 */
} redis_cluster_t;

redis_cluster_t *redis_cluster_init(const redis_conf_t *mcf, const list_t *scf);
void redis_cluster_destroy(redis_cluster_t *ctx);

bool redis_hsetnx(redisContext *ctx, const char *hash, const char *key, const char *value);

/******************************************************************************
 **函数名称: redis_rpush
 **功    能: 在链表尾插入值
 **输入参数: 
 **     ctx: Redis信息
 **     key: 链表名
 **     value: 设置值
 **输出参数:
 **返    回: 操作完成后,链表中元素总数 
 **实现描述: 
 **     RPUSH:
 **     1) 时间复杂度：O(1)
 **     2) 当 key 不存在时,该命令首先会新建一个空链表,再将数据插入到链表的头部;
 **     3) 当 key 关联的不是List类型,将返回错误信息;
 **     4) 当有多个 value 值, 那么各个 value 值按从左到右的顺序依次插入到表尾：
 **     比如对一个空列表 mylist 执行 RPUSH mylist a b c, 得出的结果列表为 a b c,
 **     等同于执行命令 RPUSH mylist a, RPUSH mylist b, RPUSH mylist c.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#define redis_rpush(ctx, key, value) redisCommand(ctx, "RPUSH %s %s", key, value)

/******************************************************************************
 **函数名称: redis_lpop
 **功    能: 移除并返回链表头元素
 **输入参数: 
 **     ctx: Redis信息
 **     key: 链表名
 **输出参数:
 **返    回: 链表头元素
 **实现描述: 
 **     LPOP:
 **     1) 时间复杂度: O(1)
 **     2) 当链表key不存在时, 将返回NIL;
 **     3) 当key不是List类型时, 将返回错误信息.
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.28 #
 ******************************************************************************/
#define redis_lpop(ctx, key) redisCommand(ctx, "LPOP %s", key)

#endif /*__HTTP_H__*/
