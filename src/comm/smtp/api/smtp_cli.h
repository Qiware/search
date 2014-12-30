#if !defined(__SMTP_CLI_H__)
#define __SMTP_CLI_H__

#include "smtp_comm.h"
#include "orm_queue.h"
#include "smtp_snd_svr.h"

#define SMTP_SND_REQ_INTV_NUM  (100) /* 每发送N个通知1次发送服务 */

/* 发送服务命令套接字 */
#define smtp_ssvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "./tmp/smtp/snd/%s/usck/%s_ssvr_%d.usck", \
        conf->name, conf->name, tidx+1)

/* 发送对象信息 */
typedef struct
{
    smtp_snd_conf_t conf;            /* 客户端配置信息 */
    
    int cmdfd;                      /* 命令套接字 */
    ORMQUEUE **sq;                  /* 发送缓冲队列 */
    unsigned int *snd_num;          /* 各队列放入计数 */
} smtp_cli_t;

extern smtp_cli_t *smtp_init_cli(const smtp_snd_conf_t *conf, int idx);
extern int smtp_cli_send(smtp_cli_t *cli, const void *data, int type, size_t size);

#endif /*__SMTP_SND_CLI_H__*/
