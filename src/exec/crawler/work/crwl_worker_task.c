#include <netdb.h>

#include "log.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_task.h"
#include "xdo_socket.h"
#include "crwl_worker.h"

/******************************************************************************
 **函数名称: crwl_worker_task_load_webpage_by_url
 **功    能: 通过URL加载网页的任务处理
 **输入参数: 
 **     worker: 爬虫对象 
 **     args: 通过URL加载网页的任务的参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
int crwl_worker_task_load_webpage_by_url(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_url_t *args)
{
    int ret, fd;
    struct hostent *host;
    crwl_worker_socket_t *sck;
    char ipaddr[IP_ADDR_MAX_LEN];
    
    /* 1. 通过URL获取WEB服务器信息 */
    host = gethostbyname(args->url);
    if (NULL == host)
    {
        log_error(worker->log, "Get host by name failed! url:%s", args->url);
        return CRWL_OK;
    }

    inet_ntop(AF_INET, host->h_addr, ipaddr, sizeof(ipaddr));

    /* 2. 连接远程WEB服务器 */
    fd = tcp_connect_ex2(ipaddr, args->port);
    if (fd < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s! ipaddr:%s url:%s",
            errno, strerror(errno), ipaddr, args->url);
        return CRWL_OK;
    }

    /* 3. 将FD等信息加入套接字链表 */
    sck = eslab_alloc(&worker->slab, sizeof(crwl_worker_socket_t));
    if (NULL == sck)
    {
        log_error(worker->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    memset(sck, 0, sizeof(crwl_worker_socket_t));

    sck->sckid = fd;
    snprintf(sck->url, sizeof(sck->url), "%s", args->url);
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", ipaddr);
    sck->port = args->port;

    ret = crwl_worker_add_sock(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add socket into list failed!");
        eslab_free(&worker->slab, sck);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_worker_task_load_webpage_by_ip
 **功    能: 通过IP加载网页的任务处理
 **输入参数: 
 **     worker: 爬虫对象 
 **     args: 通过IP加载网页的任务的参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 连接远程WEB服务器
 **     2. 将FD等信息加入套接字链表
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.28 #
 ******************************************************************************/
int crwl_worker_task_load_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_load_webpage_by_ip_t *args)
{
    int ret, fd;
    crwl_worker_socket_t *sck;

    /* 暂不支持IPV6 */
    if (IPV6 == args->type)
    {
        return CRWL_OK;
    }
    
    /* 1. 连接远程WEB服务器 */
    fd = tcp_connect_ex(args->ipaddr, args->port, CRWL_WRK_CONNECT_TMOUT);
    if (fd < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s! ipaddr:%s",
            errno, strerror(errno), args->ipaddr);
        return CRWL_OK;
    }

    /* 2. 将FD等信息加入套接字链表 */
    sck = eslab_alloc(&worker->slab, sizeof(crwl_worker_socket_t));
    if (NULL == sck)
    {
        log_error(worker->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    memset(sck, 0, sizeof(crwl_worker_socket_t));

    sck->sckid = fd;
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", args->ipaddr);
    sck->port = args->port;

    ret = crwl_worker_add_sock(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add socket into list failed!");
        eslab_free(&worker->slab, sck);
        return CRWL_ERR;
    }

    return CRWL_OK;
}
