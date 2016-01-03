#if !defined(__LSND_CONF_H__)
#define __LSND_CONF_H__

#include "comm.h"
#include "agent.h"
#include "rtsd_send.h"

/* 侦听配置 */
typedef struct
{
    char name[NODE_MAX_LEN];        /* 节点名 */
    char wdir[FILE_PATH_MAX_LEN];   /* 工作路径 */

    struct
    {
        int num;                    /* 队列数 */
        int max;                    /* 队列长度 */
        int size;                   /* 单元大小 */
    } distq;                        /* 分发队列 */
    agent_conf_t agent;             /* 代理配置 */
    rtsd_conf_t invtd_conf;         /* INVERTD连接配置 */
} lsnd_conf_t;

#define LSND_GET_DISTQ_PATH(path, size, dir, idx)   /* 分发队列路径 */\
    snprintf(path, sizeof(path), "%s/dist-%d.shmq", dir, idx);

#define LSND_GET_DSVR_CMD_PATH(path, size, dir)     /* 分发服务命令路径 */\
    snprintf(path, sizeof(path), "%s/dsvr.usck", dir);

int lsnd_load_conf(const char *name, const char *path, lsnd_conf_t *conf, log_cycle_t *log);

#endif /*__LSND_CONF_H__*/
