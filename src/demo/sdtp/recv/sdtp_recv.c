
#include "sdtp.h"

typedef enum
{
    AAAA
    , BBBB
    , CCCC
    , DDDD
    , EEEE
}MYSELT_DATA_TYPE_e;

static void sdtp_setup_conf(sdtp_conf_t *conf)
{
    snprintf(conf->name, sizeof(conf->name), "SDTP-RECV");
    conf->port = 4444;
    conf->recv_thd_num = 1;
    conf->work_thd_num = 1;
    conf->rqnum = 3;
    conf->recvq.max = 500;
    conf->recvq.size = 40960;
}

int main(int argc, const char *argv[])
{
    int ret;
    sdtp_cntx_t *ctx;
    log_cycle_t *log;
    sdtp_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    sdtp_setup_conf(&conf);

    signal(SIGPIPE, SIG_IGN);
                                       
    syslog_init(LOG_LEVEL_ERROR, "./sdtp.log2");
    log = log_init(LOG_LEVEL_ERROR, "./sdtp.log");

    /* 1. 接收端初始化 */
    ctx = sdtp_init(&conf, log);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize rcvsvr failed!");
        return SDTP_ERR;
    }

#if 0
    sdtp_register(ctx, AAAA, sdtp_worker_def_hdl, NULL);
    sdtp_register(ctx, BBBB, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, CCCC, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, DDDD, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, EEEE, sdtp_work_def_hdl, NULL);
#endif

    /* 2. 接收服务端工作 */
    ret = sdtp_startup(ctx);
    if (0 != ret)
    {
        fprintf(stderr, "Work failed!");
    }

    while(1) pause();

    /* 3. 接收端释放 */
    return sdtp_destroy(&ctx);
}
