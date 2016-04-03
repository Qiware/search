#if !defined(__LSND_CONF_H__)
#define __LSND_CONF_H__

#include "comm.h"
#include "agent.h"
#include "rtsd_send.h"

/* 侦听配置 */
typedef struct
{
    int nid;                        /* 结点ID */
    char name[NODE_MAX_LEN];        /* 结点名 */
    char wdir[FILE_PATH_MAX_LEN];   /* 工作路径 */

    struct {
        int num;                    /* 队列数 */
        int max;                    /* 队列长度 */
        int size;                   /* 单元大小 */
    } distq;                        /* 分发队列 */
    agent_conf_t agent;             /* 代理配置 */
    rtsd_conf_t invtd_conf;         /* INVERTD连接配置 */
} lsnd_conf_t;

int lsnd_load_conf(const char *path, lsnd_conf_t *conf, log_cycle_t *log);

#endif /*__LSND_CONF_H__*/
