/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crwl_task.c
 ** 版本号: 1.0
 ** 描  述: 爬虫任务的分析和处理
 **         负责将爬虫任务队列中的数据分析和处理工作
 ** 作  者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
#include <netdb.h>

#include "log.h"
#include "http.h"
#include "xd_str.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "crwl_task.h"
#include "xd_socket.h"
#include "crwl_worker.h"

/******************************************************************************
 **函数名称: crwl_task_down_webpage_by_uri
 **功    能: 通过URL加载网页的任务处理
 **输入参数: 
 **     worker: 爬虫对象 
 **     args: 通过URL加载网页的任务的参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 通过URL获取WEB服务器信息(域名, 端口号)
 **     2. 连接远程WEB服务器
 **     3. 将FD等信息加入套接字链表
 **     4. 添加HTTP GET请求
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
int crwl_task_down_webpage_by_uri(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_uri_t *args)
{
    int ret, fd, ip_idx;
    uri_field_t field;
    crwl_domain_t *domain;
    crwl_worker_socket_t *sck;

    memset(&field, 0, sizeof(field));

    /* 解析URI字串 */
    if(0 != uri_reslove(args->uri, &field))
    {
        log_error(worker->log, "Reslove uri [%s] failed!", args->uri);
        return CRWL_ERR;
    }
   
    /* 1. 通过URL获取WEB服务器IP信息 */
    domain = crwl_get_ipaddr(worker->ctx, field.host);
    if (NULL == domain
        || 0 == domain->ip_num)
    {
        log_error(worker->log, "Get ip failed! host:%s", field.host);
        return CRWL_ERR;
    }

    ip_idx = random() % domain->ip_num;

    /* 2. 连接远程WEB服务器 */
    fd = tcp_connect_ex2(domain->ip[ip_idx], args->port);
    if (fd < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s! ipaddr:%s uri:%s",
            errno, strerror(errno), domain->ip[ip_idx], args->uri);
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
    ftime(&sck->crtm);
    sck->rdtm = sck->crtm.time;
    sck->wrtm = sck->crtm.time;
    sck->read.addr = sck->recv;

    snprintf(sck->webpage.uri, sizeof(sck->webpage.uri), "%s", args->uri);
    snprintf(sck->webpage.ipaddr, sizeof(sck->webpage.ipaddr), "%s", domain->ip[ip_idx]);
    sck->webpage.port = args->port;

    ret = crwl_worker_add_sock(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add socket into list failed!");
        eslab_dealloc(&worker->slab, sck);
        return CRWL_ERR;
    }

    /* 4. 添加HTTP GET请求 */
    ret = crwl_worker_add_http_get_req(worker, sck, args->uri);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add http get request failed!");

        crwl_worker_remove_sock(worker, sck);
        return CRWL_ERR;
    }

    /* 5. 新建存储文件 */
    ret = crwl_worker_webpage_creat(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Save webpage failed!");

        crwl_worker_remove_sock(worker, sck);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_task_down_webpage_by_ip
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
int crwl_task_down_webpage_by_ip(
        crwl_worker_t *worker, const crwl_task_down_webpage_by_ip_t *args)
{
    int ret, fd;
    crwl_worker_socket_t *sck;

    /* 暂不支持IPV6 */
    if (IPV6 == args->type)
    {
        return CRWL_OK;
    }
    
    /* 1. 连接远程WEB服务器 */
    fd = tcp_connect_ex(args->ipaddr, args->port, CRWL_CONNECT_TMOUT_SEC);
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
    snprintf(sck->webpage.ipaddr, sizeof(sck->webpage.ipaddr), "%s", args->ipaddr);
    sck->webpage.port = args->port;

    ret = crwl_worker_add_sock(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add socket into list failed!");
        eslab_dealloc(&worker->slab, sck);
        return CRWL_ERR;
    }

    return CRWL_OK;
}
