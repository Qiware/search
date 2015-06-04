#if !defined(__SDTP_CLI_H__)
#define __SDTP_CLI_H__

#include "mem_pool.h"
#include "shm_queue.h"
#include "sdtp_comm.h"
#include "sdtp_send.h"

/* 发送服务命令套接字 */
#define dsnd_ssvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "../temp/sdtp/snd/%s/usck/%s_ssvr_%d.usck", conf->name, conf->name, tidx+1)

/* 发送对象信息 */
typedef struct
{
    dsnd_conf_t conf;               /* 配置信息 */
    log_cycle_t *log;               /* 日志对象 */
    mem_pool_t *pool;               /* 内存池 */
    
    int cmdfd;                      /* 命令套接字 */
    dsnd_pool_t **sendq;            /* 发送缓冲队列 */
} dsnd_cli_t;

extern dsnd_cli_t *dsnd_cli_init(const dsnd_conf_t *conf, int idx, log_cycle_t *log);
extern int dsnd_cli_send(dsnd_cli_t *cli, int type, const void *data, size_t size);

#endif /*__SDTP_CLI_H__*/
