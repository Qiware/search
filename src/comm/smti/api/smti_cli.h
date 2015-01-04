#if !defined(__SMTI_CLI_H__)
#define __SMTI_CLI_H__

#include "smti_comm.h"
#include "orm_queue.h"
#include "smti_snd_svr.h"

#define SMTI_SND_REQ_INTV_NUM  (100) /* 每发送N个通知1次发送服务 */

/* 发送服务命令套接字 */
#define smti_ssvr_usck_path(conf, path, tidx) \
    snprintf(path, sizeof(path), "./tmp/smti/snd/%s/usck/%s_ssvr_%d.usck", \
        conf->name, conf->name, tidx+1)

/* 发送对象信息 */
typedef struct
{
    smti_snd_conf_t conf;           /* 客户端配置信息 */
    
    int cmdfd;                      /* 命令套接字 */
    ORMQUEUE **sq;                  /* 发送缓冲队列 */
    unsigned int *snd_num;          /* 各队列放入计数 */
} smti_cli_t;

extern smti_cli_t *smti_init_cli(const smti_snd_conf_t *conf, int idx);
extern int smti_cli_send(smti_cli_t *cli, const void *data, int type, size_t size);

#endif /*__SMTI_SND_CLI_H__*/
