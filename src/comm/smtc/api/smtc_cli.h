#if !defined(__SMTC_CLI_H__)
#define __SMTC_CLI_H__

#include "shm_queue.h"
#include "smtc_priv.h"
#include "smtc_ssvr.h"

#define SMTC_SND_REQ_INTV_NUM  (100) /* 每发送N个通知1次发送服务 */

/* 发送服务命令套接字 */
#define smtc_ssvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "./tmp/smtc/snd/%s/usck/%s_ssvr_%d.usck", \
        conf->name, conf->name, tidx+1)

/* 发送对象信息 */
typedef struct
{
    smtc_ssvr_conf_t conf;          /* 客户端配置信息 */
    
    int cmdfd;                      /* 命令套接字 */
    shm_queue_t **sq;               /* 发送缓冲队列 */
    unsigned int *snd_num;          /* 各队列放入计数 */
} smtc_cli_t;

extern smtc_cli_t *smtc_init_cli(const smtc_ssvr_conf_t *conf, int idx);
extern int smtc_cli_send(smtc_cli_t *cli, const void *data, int type, size_t size);

#endif /*__SMTC_SND_CLI_H__*/
