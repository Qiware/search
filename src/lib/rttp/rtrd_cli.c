#include "mesg.h"
#include "rtrd_recv.h"

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
rtrd_cli_t *rtrd_cli_init(const rtrd_conf_t *conf)
{
    rtrd_cli_t *cli;

    /* > 创建对象 */
    cli = (rtrd_cli_t *)calloc(1, sizeof(rtrd_cli_t));
    if (NULL == cli)
    {
        return NULL;
    }

    /* > 附着共享内存队列 */
    cli->sendq = rtrd_shm_sendq_attach(conf);
    if (NULL == cli->sendq)
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
int rtrd_cli_send(rtrd_cli_t *cli, int type, int dest, void *data, int len)
{
    void *addr;
    rttp_frwd_t *frwd;

    /* > 合法性检测 */
    if (len > (int)sizeof(rttp_frwd_t) + shm_queue_size(cli->sendq))
    {
        return -1;
    }

    /* > 申请队列空间 */
    addr = shm_queue_malloc(cli->sendq);
    if (NULL == addr)
    {
        return RTTP_ERR;
    }

    frwd = (rttp_frwd_t *)addr;

    frwd->type = type; 
    frwd->dest_nodeid = dest;
    frwd->length = len;

    memcpy(addr+sizeof(rttp_frwd_t), data, len);

    /* > 压入队列空间 */
    if (shm_queue_push(cli->sendq, addr))
    {
        shm_queue_dealloc(cli->sendq, addr);
    }

    //rtrd_cli_cmd_dist_req(cli, idx);

    return RTTP_OK;
}
