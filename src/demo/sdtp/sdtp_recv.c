
#include "sdtp.h"
#include "mesg.h"

/* 回调函数 */
static int sdtp_work_def_hdl(int type, char *buff, size_t len, void *args)
{
    fprintf(stderr, "type:%d buff:%p len:%ld args:%p\n", type, buff, len, args);
    return 0;
}

/* 配置SDTP */
static void sdtp_setup_conf(sdtp_conf_t *conf, int port)
{
    snprintf(conf->name, sizeof(conf->name), "SDTP-RECV");
    conf->port = port;
    conf->recv_thd_num = 1;
    conf->work_thd_num = 1;
    conf->rqnum = 3;
    conf->recvq.max = 1024;
    conf->recvq.size = 40960;
}

int main(int argc, const char *argv[])
{
    int ret, port;
    sdtp_cntx_t *ctx;
    log_cycle_t *log;
    sdtp_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    if (2 != argc)
    {
        fprintf(stderr, "Didn't special port!");
        return -1;
    }

    port = atoi(argv[1]);

    sdtp_setup_conf(&conf, port);

    signal(SIGPIPE, SIG_IGN);
                                       
    plog_init(LOG_LEVEL_ERROR, "./sdtp.log2");
    log = log_init(LOG_LEVEL_ERROR, "./sdtp.log");

    /* 1. 接收端初始化 */
    ctx = sdtp_init(&conf, log);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize rcvsvr failed!");
        return SDTP_ERR;
    }

    sdtp_register(ctx, MSG_SEARCH_REQ, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, MSG_PRINT_INVT_TAB_REQ, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, MSG_QUERY_CONF_REQ, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, MSG_QUERY_WORKER_STAT_REQ, sdtp_work_def_hdl, NULL);
    sdtp_register(ctx, MSG_QUERY_WORKQ_STAT_REQ, sdtp_work_def_hdl, NULL);

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
