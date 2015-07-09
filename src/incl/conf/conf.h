#if !defined(__CONF_H__)
#define __CONF_H__

#include "comm.h"
#include "list.h"

#define SYS_CONF_DEF_PATH "../conf/sys_conf.xml"

/* 日志配置 */
typedef struct
{
    size_t log_max_size;                /* 日志文件大小 */
    int sync_thd_num;                   /* 同步线程个数 */
    int cache_max_num;                  /* 缓存最大个数 */
    size_t cache_size;                  /* 缓存单元大小 */
    int cache_timeout;                  /* 缓存超时时间 */
} log_conf_t;

/* 配置映射 */
typedef struct
{
    char name[NODE_MAX_LEN];            /* 结点名 */
    char path[FILE_PATH_MAX_LEN];       /* 结点对应的配置路径 */
} conf_map_t;

/* 系统配置 */
typedef struct
{
    log_conf_t log;                     /* 日志配置 */
    list_t *listen;                     /* 侦听配置 */
    list_t *frwder;                     /* 转发配置 */
} sys_conf_t;

int conf_load(const char *fpath, sys_conf_t *conf);

#endif /*__CONF_H__*/
