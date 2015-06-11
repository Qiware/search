#include "rttp_cmd.h"
#include "rttp_comm.h"
#include "rtrd_recv.h"

/* 分发对象 */
typedef struct
{
    int cmd_sck_id;         /* 命令套接字 */
} rtrd_dist_t;

static rtrd_dist_t *rtrd_dist_init(rtrd_cntx_t *ctx);
static int rtrd_dist_cmd_send_req(rtrd_cntx_t *ctx, rtrd_dist_t *dist, int idx);

/******************************************************************************
 **函数名称: rtrd_dist_routine
 **功    能: 运行分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
void *rtrd_dist_routine(void *_ctx)
{
    int idx;
    void *data, *addr;
    rttp_frwd_t *frwd;
    rtrd_dist_t *dist;
    rtrd_cntx_t *ctx = (rtrd_cntx_t *)_ctx;

    /* > 初始化分发线程 */
    dist = rtrd_dist_init(ctx);
    if (NULL == dist)
    {
        abort();
    }

    while (1)
    {
        /* > 弹出发送数据 */
        data = shm_queue_pop(ctx->shm_sendq);
        if (NULL == data)
        {
            usleep(500); /* TODO: 可使用事件通知机制减少CPU的消耗 */
            continue;
        }

        /* > 获取发送队列 */
        frwd = (rttp_frwd_t *)data;

        idx = rtrd_node_to_svr_map_rand(ctx, frwd->dest_nodeid);
        if (idx < 0)
        {
            log_error(ctx->log, "Didn't find dev to svr map! nodeid:%d", frwd->dest_nodeid);
            continue;
        }

        /* > 获取发送队列 */
        addr = queue_malloc(ctx->sendq[idx]);
        if (NULL == addr)
        {
            shm_queue_dealloc(ctx->shm_sendq, data);
            log_error(ctx->log, "Alloc memory from queue failed!");
            continue;
        }

        memcpy(addr, data, frwd->length);

        queue_push(ctx->sendq[idx], addr);

        shm_queue_dealloc(ctx->shm_sendq, data);

        /* > 发送发送请求 */
        rtrd_dist_cmd_send_req(ctx, dist, idx);
    }

    return (void *)-1;
}

/******************************************************************************
 **函数名称: rtrd_dist_init
 **功    能: 初始化分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发线程
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.09 15:53:03 #
 ******************************************************************************/
static rtrd_dist_t *rtrd_dist_init(rtrd_cntx_t *ctx)
{
    rtrd_dist_t *dist;
    char path[FILE_NAME_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    /* > 创建对象 */
    dist = (rtrd_dist_t *)calloc(1, sizeof(rtrd_dist_t));
    if (NULL == dist)
    {
        log_error(ctx->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* > 创建通信套接字 */
    rtrd_dist_unix_path(conf, path);

    dist->cmd_sck_id = unix_udp_creat(path);
    if (dist->cmd_sck_id < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        free(dist);
        return NULL;
    }

    return dist;
}

/******************************************************************************
 **函数名称: rtrd_dist_cmd_send_req
 **功    能: 发送发送请求
 **输入参数:
 **     ctx: 全局信息
 **     dist: 分发信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.09 15:53:03 #
 ******************************************************************************/
static int rtrd_dist_cmd_send_req(rtrd_cntx_t *ctx, rtrd_dist_t *dist, int idx)
{
    rttp_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTTP_CMD_DIST_REQ;

    rtrd_rsvr_usck_path(conf, path, idx);

    return unix_udp_send(dist->cmd_sck_id, path, &cmd, sizeof(cmd));
}
