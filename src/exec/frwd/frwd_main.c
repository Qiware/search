#include <signal.h>
#include <sys/time.h>

#include "mesg.h"
#include "syscall.h"
#include "sdtp_cli.h"
#include "sdtp_ssvr.h"

int sdtp_setup_conf(sdtp_ssvr_conf_t *conf, int port);
int frwd_set_reg(sdtp_sctx_t *ctx);

/* 主函数 */
int main(int argc, const char *argv[])
{
    int port;
    log_cycle_t *log;
    sdtp_sctx_t *ctx;
    sdtp_ssvr_conf_t conf;

    if (2 != argc)
    {
        fprintf(stderr, "Didn't special port!");
        return -1;
    }

    memset(&conf, 0, sizeof(conf));

    signal(SIGPIPE, SIG_IGN);

    nice(-20);

    port = atoi(argv[1]);
    sdtp_setup_conf(&conf, port);

    plog_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.plog");
    log = log_init(LOG_LEVEL_DEBUG, "./sdtp_ssvr.log");
    if (NULL == log)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* > 初始化SDTP发送服务 */
    ctx = sdtp_send_init(&conf, log);
    if (NULL == ctx) 
    {
        fprintf(stderr, "Initialize send-server failed!");
        return -1;
    }

    /* > 注册SDTP回调 */
    if (frwd_set_reg(ctx))
    {
        fprintf(stderr, "Register callback failed!");
        return -1;
    }

    /* > 启动SDTP发送服务 */
    if (sdtp_send_start(ctx))
    {
        fprintf(stderr, "Start up send-server failed!");
        return -1;
    }

    while (1) { pause(); }

    fprintf(stderr, "Exit send server!");

    return 0;
}

/* 设置SDTP配置 */
int sdtp_setup_conf(sdtp_ssvr_conf_t *conf, int port)
{
    conf->devid = 1;
    snprintf(conf->name, sizeof(conf->name), "SDTP-SEND");

    snprintf(conf->auth.usr, sizeof(conf->auth.usr), "qifeng");
    snprintf(conf->auth.passwd, sizeof(conf->auth.passwd), "111111");

    snprintf(conf->ipaddr, sizeof(conf->ipaddr), "127.0.0.1");

    conf->port = port;
    conf->send_thd_num = 1;
    conf->send_buff_size = 5 * MB;
    conf->recv_buff_size = 2 * MB;

    snprintf(conf->sendq.name, sizeof(conf->sendq.name), "../temp/sdtp/sdtp-ssvr.key");
    conf->sendq.size = 4096;
    conf->sendq.count = 2048;
    return 0;
}

int frwd_search_rep_hdl(int type, int orig, char *data, size_t len, void *args)
{
    return 0;
}

/* 注册处理回调 */
int frwd_set_reg(sdtp_sctx_t *ctx)
{
    sdtp_send_register(ctx, MSG_SEARCH_REP, frwd_search_rep_hdl, NULL);
    return 0;
}
