#include "comm.h"
#include "mesg.h"
#include "frwd.h"
#include "agent.h"
#include "syscall.h"
#include "agent_mesg.h"

int sdtp_setup_conf(sdsd_conf_t *conf, int port);
int frwd_set_reg(frwd_cntx_t *frwd);

/* 主函数 */
int main(int argc, const char *argv[])
{
    frwd_cntx_t *frwd;
    char path[FILE_PATH_MAX_LEN];

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
    sdtp_setup_conf(&frwd->conf.sdtp, atoi(argv[1]));

    snprintf(path, sizeof(path), "../log/%s.plog", basename(argv[0]));
    plog_init(LOG_LEVEL_TRACE, path);

    snprintf(path, sizeof(path), "../log/%s.log", basename(argv[0]));
    frwd->log = log_init(LOG_LEVEL_TRACE, path);
    if (NULL == frwd->log)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* > 发送队列 */
    frwd->send_to_agentd = shm_queue_attach(AGTD_SHM_SENDQ_PATH);
    if (NULL == frwd->send_to_agentd)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    /* > 初始化SDTP发送服务 */
    frwd->sdtp = sdsd_init(&frwd->conf.sdtp, frwd->log);
    if (NULL == frwd->sdtp) 
    {
        fprintf(stderr, "Initialize send-server failed!");
        return -1;
    }

    /* > 注册SDTP回调 */
    if (frwd_set_reg(frwd))
    {
        fprintf(stderr, "Register callback failed!");
        return -1;
    }

    /* > 启动SDTP发送服务 */
    if (sdsd_start(frwd->sdtp))
    {
        fprintf(stderr, "Start up send-server failed!");
        return -1;
    }

    while (1) { pause(); }

    fprintf(stderr, "Exit send server!");

    return 0;
}

/* 设置SDTP配置 */
int sdtp_setup_conf(sdsd_conf_t *conf, int port)
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

/* 转发到指定SHMQ队列 */
static int frwd_shmq_push(shm_queue_t *shmq, int type, int orig, char *data, size_t len)
{
    void *addr;
    size_t size;
    sdtp_header_t *head;

    size = sizeof(sdtp_header_t) + shm_queue_size(shmq);
    if (size < len)
    {
        return -1;
    }

    /* > 申请队列空间 */
    addr = shm_queue_malloc(shmq);
    if (NULL == addr)
    {
        return -1;
    }

    /* > 设置应答数据 */
    head = (sdtp_header_t *)addr;

    head->type = type;
    head->devid = orig;
    head->length = len;
    head->flag = SDTP_EXP_MESG;
    head->checksum = SDTP_CHECK_SUM;

    memcpy(head+1, data, len);

    if (shm_queue_push(shmq, addr))
    {
        shm_queue_dealloc(shmq, addr);
        return -1;
    }

    return 0;
}

/* 搜索应答处理 */
int frwd_search_rep_hdl(int type, int orig, char *data, size_t len, void *args)
{
    frwd_cntx_t *ctx = (frwd_cntx_t *)args;

    log_trace(ctx->log, "Call %s()", __func__);

    return frwd_shmq_push(ctx->send_to_agentd, type, orig, data, len);
}

/* 注册处理回调 */
int frwd_set_reg(frwd_cntx_t *frwd)
{
    sdsd_register(frwd->sdtp, MSG_SEARCH_REP, frwd_search_rep_hdl, frwd);
    return 0;
}
