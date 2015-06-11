#if !defined(__RTSD_CLI_H__)
#define __RTSD_CLI_H__

#include "mem_pool.h"
#include "shm_queue.h"
#include "rttp_comm.h"
#include "rtsd_send.h"

/* 发送服务命令套接字 */
#define rtsd_ssvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "%s/%d_ssvr_%d.usck", conf->path, conf->nodeid, tidx+1)

/* 发送对象信息 */
typedef struct
{
    rtsd_conf_t conf;               /* 配置信息 */
    log_cycle_t *log;               /* 日志对象 */
    mem_pool_t *pool;               /* 内存池 */
    
    int cmd_sck_id;                 /* 命令套接字 */
    shm_queue_t **sendq;            /* 发送缓冲队列 */
} rtsd_cli_t;

extern rtsd_cli_t *rtsd_cli_init(const rtsd_conf_t *conf, int idx, log_cycle_t *log);
extern int rtsd_cli_send(rtsd_cli_t *cli, int type, const void *data, size_t size);

#endif /*__RTSD_CLI_H__*/
