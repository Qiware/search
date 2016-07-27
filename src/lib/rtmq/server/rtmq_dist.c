#include "comm.h"
#include "mesg.h"
#include "mem_ref.h"
#include "rtmq_comm.h"
#include "rtmq_recv.h"

/* 分发对象 */
typedef struct
{
    int cmd_sck_id;                     /* 命令套接字 */
    log_cycle_t *log;                   /* 日志对象 */
} rtmq_dsvr_t;

static rtmq_dsvr_t *rtmq_dsvr_init(rtmq_cntx_t *ctx);
static int rtmq_dsvr_dist_data_hdl(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr);
static int rtmq_dsvr_cmd_recv_and_proc(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr);
static int rtmq_dsvr_cmd_dist_req(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr, int idx);

/******************************************************************************
 **函数名称: rtmq_dsvr_routine
 **功    能: 运行分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
void *rtmq_dsvr_routine(void *_ctx)
{
    int ret;
    fd_set rdset;
    rtmq_dsvr_t *dsvr;
    struct timeval timeout;
    rtmq_cntx_t *ctx = (rtmq_cntx_t *)_ctx;

    /* > 初始化分发线程 */
    dsvr = rtmq_dsvr_init(ctx);
    if (NULL == dsvr) {
        abort();
    }

    nice(-20);

    while (1) {
        FD_ZERO(&rdset);

        FD_SET(dsvr->cmd_sck_id, &rdset);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ret = select(dsvr->cmd_sck_id+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            log_error(dsvr->log, "errno:[%d] %s!", errno, strerror(errno));
            continue;
        }
        else if (0 == ret) {
            rtmq_dsvr_dist_data_hdl(ctx, dsvr);
            continue;
        }

        if (FD_ISSET(dsvr->cmd_sck_id, &rdset)) {
            rtmq_dsvr_cmd_recv_and_proc(ctx, dsvr);
        }
    }

    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtmq_dsvr_init
 **功    能: 初始化分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发线程
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.09 15:53:03 #
 ******************************************************************************/
static rtmq_dsvr_t *rtmq_dsvr_init(rtmq_cntx_t *ctx)
{
    rtmq_dsvr_t *dsvr;
    char path[FILE_NAME_MAX_LEN];
    rtmq_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    dsvr = (rtmq_dsvr_t *)calloc(1, sizeof(rtmq_dsvr_t));
    if (NULL == dsvr) {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    dsvr->log = ctx->log;

    /* > 创建通信套接字 */
    rtmq_dsvr_usck_path(conf, path);

    dsvr->cmd_sck_id = unix_udp_creat(path);
    if (dsvr->cmd_sck_id < 0) {
        log_error(dsvr->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        free(dsvr);
        return NULL;
    }

    return dsvr;
}

/******************************************************************************
 **函数名称: rtmq_dsvr_cmd_dist_req
 **功    能: 发送分发请求
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将分发请求发送至接收线程
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.09 15:53:03 #
 ******************************************************************************/
static int rtmq_dsvr_cmd_dist_req(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr, int idx)
{
    rtmq_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTMQ_CMD_DIST_REQ;

    rtmq_rsvr_usck_path(&ctx->conf, path, idx);

    return (unix_udp_send(dsvr->cmd_sck_id, path, &cmd, sizeof(cmd)) > 0)? 0 : -1;
}

/******************************************************************************
 **函数名称: rtmq_dsvr_dist_data_hdl
 **功    能: 数据分发处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: WARNNING: 千万勿将共享变量参与MIN()三目运算, 否则可能出现严重错误!!!!且很难找出原因!
 **          原因: MIN()不是原子运算, 使用共享变量可能导致判断成立后, 而返回时共
 **                享变量的值可能被其他进程或线程修改, 导致出现严重错误!
 **作    者: # Qifeng.zou # 2015.06.13 #
 ******************************************************************************/
static int rtmq_dsvr_dist_data_hdl(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr)
{
#define RTRD_DISP_POP_NUM   (1024)
    int idx, k, num, d;
    rtmq_header_t *head;
    void *data[RTRD_DISP_POP_NUM];

    for (d=0; d<ctx->conf.distq_num; ++d) {
        /* > 计算弹出个数(WARNNING: 勿将共享变量参与MIN()三目运算, 否则可能出现严重错误!!!) */
        num = MIN(ring_used(ctx->distq[d]), RTRD_DISP_POP_NUM);
        if (0 == num) {
            continue;
        }

        /* > 弹出发送数据 */
        num = ring_mpop(ctx->distq[d], data, num);
        if (0 == num) {
            continue;
        }

        log_trace(ctx->log, "Multi-pop num:%d!", num);

        /* > 放入发送队列 */
        for (k=0; k<num; ++k) {
            /* > 获取发送队列 */
            head = (rtmq_header_t *)data[k];

            idx = rtmq_node_to_svr_map_rand(ctx, head->nid);
            if (idx < 0) {
                mem_ref_decr(data[k]);
                log_error(ctx->log, "Didn't find dev to svr map! nodeid:%d", head->nid);
                continue;
            }

            /* > 放入发送队列 */
            if (ring_push(ctx->sendq[idx], data[k])) {
                mem_ref_decr(data[k]);
                log_error(ctx->log, "Didn't find dev to svr map! nodeid:%d", head->nid);
                continue;
            }

            /* > 发送分发请求 */
            rtmq_dsvr_cmd_dist_req(ctx, dsvr, idx);
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_dsvr_cmd_recv_and_proc
 **功    能: 接收命令并进行处理
 **输入参数:
 **     ctx: 全局信息
 **     dsvr: 分发信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 接收命令并进行处理
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.13 #
 ******************************************************************************/
static int rtmq_dsvr_cmd_recv_and_proc(rtmq_cntx_t *ctx, rtmq_dsvr_t *dsvr)
{
    rtmq_cmd_t cmd;

    /* > 接收命令 */
    if (unix_udp_recv(dsvr->cmd_sck_id, &cmd, sizeof(cmd)) < 0) {
        log_error(dsvr->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    log_trace(dsvr->log, "Recv command! type:%d", cmd.type);

    /* > 处理命令 */
    switch (cmd.type) {
        case RTMQ_CMD_DIST_REQ:
        {
            return rtmq_dsvr_dist_data_hdl(ctx, dsvr);
        }
        default:
        {
            log_error(dsvr->log, "Unknown command! type:%d", cmd.type);
            return RTMQ_ERR;
        }
    }

    log_error(dsvr->log, "Unknown command! type:%d", cmd.type);
    return RTMQ_ERR;
}
