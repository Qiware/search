
#include "mesg.h"
#include "rtrd_recv.h"

/* 回调函数 */
static int rtmq_work_def_hdl(int type, int nodeid, char *buff, size_t len, void *args)
{
    fprintf(stderr, "type:%d nodeid:%d buff:%p len:%ld args:%p\n", type, nodeid, buff, len, args);
    return 0;
}

/* 配置RTMQ */
static void rtmq_setup_conf(rtrd_conf_t *conf, int port)
{
    conf->port = port;
    conf->recv_thd_num = 1;
    conf->work_thd_num = 1;
    conf->recvq_num = 3;
    conf->recvq.max = 1024;
    conf->recvq.size = 40960;
    conf->distq_num = 3;
    conf->distq.max = 1024;
    conf->distq.size = 40960;
}

int main(int argc, const char *argv[])
{
    int ret, port;
    rtrd_cntx_t *ctx;
    log_cycle_t *log;
    rtrd_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    if (2 != argc)
    {
        fprintf(stderr, "Didn't special port!");
        return -1;
    }

    port = atoi(argv[1]);

    rtmq_setup_conf(&conf, port);

    signal(SIGPIPE, SIG_IGN);
                                       
    log = log_init(LOG_LEVEL_ERROR, "./rtmq.log", "../temp/log.key");

    /* 1. 接收端初始化 */
    ctx = rtrd_init(&conf, log);
    if (NULL == ctx)
    {
        fprintf(stderr, "Initialize rcvsvr failed!");
        return RTMQ_ERR;
    }

    rtrd_register(ctx, MSG_SEARCH_WORD_REQ, rtmq_work_def_hdl, NULL);
    rtrd_register(ctx, MSG_PRINT_INVT_TAB_REQ, rtmq_work_def_hdl, NULL);
    rtrd_register(ctx, MSG_QUERY_CONF_REQ, rtmq_work_def_hdl, NULL);
    rtrd_register(ctx, MSG_QUERY_WORKER_STAT_REQ, rtmq_work_def_hdl, NULL);
    rtrd_register(ctx, MSG_QUERY_WORKQ_STAT_REQ, rtmq_work_def_hdl, NULL);

    /* 2. 接收服务端工作 */
    ret = rtrd_launch(ctx);
    if (0 != ret)
    {
        fprintf(stderr, "Work failed!");
    }

    while(1) pause();

    return 0;
}
