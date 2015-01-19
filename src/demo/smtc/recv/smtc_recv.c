#include <memory.h>
#include <assert.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "smtc.h"


#define SMTC_LOG_LEVEL   (2)
#define SMTC_LOG_SIZE    (100*1024*1024)
#define SMTC_LOG_COUNT   (10)
#define SMTC_LOG_PATH    ("./smtc_rcv.log")
#define SMTC_LOG_TYPE    (LOG_TO_FILE)

typedef enum
{
    AAAA
    , BBBB
    , CCCC
    , DDDD
    , EEEE
}MYSELT_DATA_TYPE_e;

static void smtc_setup_conf(smtc_conf_t *conf)
{
    snprintf(conf->name, sizeof(conf->name), "SMTC-RECV");
    conf->port = 54321;
    conf->recv_thd_num = 1;
    conf->work_thd_num = 3;
    conf->rqnum = 3;
    conf->recvq.max = 5000;
    conf->recvq.size = 4096;
}

int main(int argc, const char *argv[])
{
    int ret;
    smtc_cntx_t *ctx;
    log_cycle_t *log;
    smtc_conf_t conf;

    memset(&conf, 0, sizeof(conf));

    smtc_setup_conf(&conf);

    signal(SIGPIPE, SIG_IGN);
                                       
    log2_init(LOG_LEVEL_DEBUG, "./smtc.log2");
    log = log_init(LOG_LEVEL_DEBUG, "./smtc.log");

    /* 1. 接收端初始化 */
    ctx = smtc_init(&conf, log);
    if(NULL == ctx)
    {
        fprintf(stderr, "Initialize rcvsvr failed!");
        return SMTC_ERR;
    }

#if 0
    smtc_register(ctx, AAAA, smtc_worker_def_hdl, NULL);
    smtc_register(ctx, BBBB, smtc_work_def_hdl, NULL);
    smtc_register(ctx, CCCC, smtc_work_def_hdl, NULL);
    smtc_register(ctx, DDDD, smtc_work_def_hdl, NULL);
    smtc_register(ctx, EEEE, smtc_work_def_hdl, NULL);
#endif

    /* 2. 接收服务端工作 */
    ret = smtc_startup(ctx);
    if(0 != ret)
    {
        fprintf(stderr, "Work failed!");
    }

    while(1) pause();

    /* 3. 接收端释放 */
    return smtc_destroy(&ctx);
}
