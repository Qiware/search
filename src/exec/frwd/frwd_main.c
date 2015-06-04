#include <signal.h>
#include <sys/time.h>

#include "mesg.h"
#include "syscall.h"
#include "dsnd_cli.h"
#include "dsnd_ssvr.h"

/* 配置信息 */
typedef struct
{
    dsnd_conf_t sdtp;      /* SDTP配置 */
} frwd_conf_t;

/* 全局对象 */
typedef struct
{
    frwd_conf_t conf;           /* 配置信息 */
    log_cycle_t *log;           /* 日志对象 */
    dsnd_cntx_t *sdtp;          /* SDTP对象 */
} frwd_cntx_t;

int sdtp_setup_conf(dsnd_conf_t *conf, int port);
int frwd_set_reg(dsnd_cntx_t *ctx);

/* 主函数 */
int main(int argc, const char *argv[])
{
    int port;
    frwd_cntx_t *frwd;

    if (2 != argc)
    {
        fprintf(stderr, "Didn't special port!");
        return -1;
    }

    /* > 创建全局对象 */
    frwd = (frwd_cntx_t *)calloc(1, sizeof(frwd_cntx_t));
    if (NULL == frwd)
    {
        fprintf(stderr, "errmsg:[%d] %s!\n", errno, strerror(errno));
        return -1;
    }

    memset(&frwd->conf, 0, sizeof(frwd->conf));

    /* > 初始化日志 */
    port = atoi(argv[1]);
    sdtp_setup_conf(&frwd->conf.sdtp, port);

    plog_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.plog");
    frwd->log = log_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.log");
    if (NULL == frwd->log)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* > 初始化SDTP发送服务 */
    frwd->sdtp = dsnd_init(&frwd->conf.sdtp, frwd->log);
    if (NULL == frwd->sdtp) 
    {
        fprintf(stderr, "Initialize send-server failed!");
        return -1;
    }

    /* > 注册SDTP回调 */
    if (frwd_set_reg(frwd->sdtp))
    {
        fprintf(stderr, "Register callback failed!");
        return -1;
    }

    /* > 启动SDTP发送服务 */
    if (dsnd_start(frwd->sdtp))
    {
        fprintf(stderr, "Start up send-server failed!");
        return -1;
    }

    while (1) { pause(); }

    fprintf(stderr, "Exit send server!");

    return 0;
}

/* 设置SDTP配置 */
int sdtp_setup_conf(dsnd_conf_t *conf, int port)
{
    conf->devid = 1;
    snprintf(conf->name, sizeof(conf->name), "SDTP-SEND");

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "qifeng");
    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "111111");

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "127.0.0.1");

    conf->port = port;
    conf->send_thd_num = 1;
    conf->work_thd_num = 1;
    conf->send_buff_size = 5 * MB;
    conf->recv_buff_size = 2 * MB;

    snprintf(conf->sendq.name, sizeof(conf->sendq.name), "../temp/sdtp/sdtp-ssvr.key");
    conf->sendq.size = 4096;
    conf->sendq.count = 2048;

    conf->recvq.max = 4096;
    conf->recvq.size = 2048;
    return 0;
}

/* 搜索应答处理 */
int frwd_search_rep_hdl(int type, int orig, char *data, size_t len, void *args)
{
    return 0;
}

/* 注册处理回调 */
int frwd_set_reg(dsnd_cntx_t *ctx)
{
    dsnd_register(ctx, MSG_SEARCH_REP, frwd_search_rep_hdl, NULL);
    return 0;
}
