#include "mesg.h"
#include "rtrd_recv.h"

static int rtrd_cli_cmd_dist_req(rtrd_cli_t *cli);

/******************************************************************************
 **函数名称: rtrd_cli_init
 **功    能: 初始化接收客户端
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 接收客户端
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
rtrd_cli_t *rtrd_cli_init(const rtrd_conf_t *conf, int idx)
{
    rtrd_cli_t *cli;
    char path[FILE_NAME_MAX_LEN];

    /* > 创建对象 */
    cli = (rtrd_cli_t *)calloc(1, sizeof(rtrd_cli_t));
    if (NULL == cli)
    {
        return NULL;
    }

    memcpy(&cli->conf, conf, sizeof(rtrd_conf_t));

    /* > 附着共享内存队列 */
    cli->sendq = rtrd_shm_sendq_attach(conf);
    if (NULL == cli->sendq)
    {
        free(cli);
        return NULL;
    }

    /* > 创建通信套接字 */
    rtrd_cli_unix_path(conf, path, idx);

    cli->cmd_sck_id = unix_udp_creat(path);
    if (cli->cmd_sck_id < 0)
    {
        free(cli);
        return NULL;
    }

    return cli;
}

/******************************************************************************
 **函数名称: rtrd_cli_send
 **功    能: 接收客户端发送数据
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将数据放入应答队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
int rtrd_cli_send(rtrd_cli_t *cli, int type, int dest, void *data, size_t len)
{
    void *addr;
    rttp_frwd_t *frwd;

    /* > 申请队列空间 */
    addr = shm_queue_malloc(cli->sendq, sizeof(rttp_frwd_t)+len);
    if (NULL == addr)
    {
        return RTTP_ERR;
    }

    frwd = (rttp_frwd_t *)addr;

    frwd->type = type; 
    frwd->dest = dest;
    frwd->length = len;

    memcpy(addr+sizeof(rttp_frwd_t), data, len);

    /* > 压入队列空间 */
    if (shm_queue_push(cli->sendq, addr))
    {
        shm_queue_dealloc(cli->sendq, addr);
    }

    rtrd_cli_cmd_dist_req(cli);

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_cli_cmd_dist_req
 **功    能: 通知分发服务
 **输入参数:
 **     cli: 上下文信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.03.20 #
 ******************************************************************************/
static int rtrd_cli_cmd_dist_req(rtrd_cli_t *cli)
{
    rttp_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    rtrd_conf_t *conf = &cli->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTTP_CMD_DIST_REQ;

    rtrd_dsvr_usck_path(conf, path);

    return unix_udp_send(cli->cmd_sck_id, path, &cmd, sizeof(cmd));
}


