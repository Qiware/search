#include "mesg.h"
#include "sdtp_recv.h"

/******************************************************************************
 **函数名称: sdtp_rcli_init
 **功    能: 初始化接收客户端
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 接收客户端
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
sdtp_rcli_t *sdtp_rcli_init(const sdtp_conf_t *conf)
{
    sdtp_rcli_t *cli;

    /* > 创建对象 */
    cli = (sdtp_rcli_t *)calloc(1, sizeof(sdtp_rcli_t));
    if (NULL == cli)
    {
        return NULL;
    }

    /* > 附着共享内存队列 */
    cli->sendq = sdtp_shm_sendq_attach(conf);
    if (NULL == cli->sendq)
    {
        free(cli);
        return NULL;
    }

    return cli;
}

/******************************************************************************
 **函数名称: sdtp_rcli_send
 **功    能: 接收客户端发送数据
 **输入参数:
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将数据放入应答队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
int sdtp_rcli_send(sdtp_rcli_t *cli, int dest_devid, void *data, int len)
{
    void *addr;
    mesg_route_t *route;

    addr = shm_queue_malloc(cli->sendq);
    if (NULL == addr)
    {
        return -1;
    }

    route = (mesg_route_t *)addr;

    route->dest_devid = dest_devid;
    route->length = len + sizeof(mesg_route_t);

    memcpy(addr, route, sizeof(mesg_route_t));
    memcpy(addr+sizeof(mesg_route_t), data, len);

    if (shm_queue_push(cli->sendq, addr))
    {
        shm_queue_dealloc(cli->sendq, addr);
    }

    return 0;
}
