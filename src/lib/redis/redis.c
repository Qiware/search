#include "log.h"
#include "str.h"
#include "comm.h"
#include "redis.h"

/******************************************************************************
 **函数名称: redis_init
 **功    能: 初始化Redis
 **输入参数: 
 **     conf: Redis配置数组
 **输出参数:
 **返    回: Redis对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.04 #
 ******************************************************************************/
redisContext *redis_init(const redis_conf_t *conf, int sec)
{
    struct timeval tv;

    tv.tv_sec = sec;
    tv.tv_usec = 0;

    return redisConnectWithTimeout(conf->ip, conf->port, tv);
}
