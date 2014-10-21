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
 **函数名称: crwl_task_queue_init
 **功    能: 初始化任务队列
 **输入参数: 
 **     tq: 任务队列
 **     max: 队列大小
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 初始化线程读写锁
 **     2. 创建队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int crwl_task_queue_init(crwl_task_queue_t *tq, int max)
{
    int ret;

    pthread_rwlock_init(&tq->lock, NULL);

    ret = queue_init(&tq->queue, max);
    if (0 != ret)
    {
        pthread_rwlock_destroy(&tq->lock);
        return CRWL_ERR;
    }

    return CRWL_OK;
}

/******************************************************************************
 **函数名称: crwl_task_queue_push
 **功    能: 放入任务队列
 **输入参数: 
 **     tq: 任务队列
 **     addr: 数据地址
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
int crwl_task_queue_push(crwl_task_queue_t *tq, void *addr)
{
    int ret;

    pthread_rwlock_wrlock(&tq->lock);
    ret = queue_push(&tq->queue, addr);
    pthread_rwlock_unlock(&tq->lock);

    return (ret? CRWL_ERR : CRWL_OK);
}

/******************************************************************************
 **函数名称: crwl_task_queue_pop
 **功    能: 弹出任务队列
 **输入参数: 
 **     tq: 任务队列
 **输出参数: NONE
 **返    回: 数据地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void *crwl_task_queue_pop(crwl_task_queue_t *tq)
{
    void *addr;

    pthread_rwlock_wrlock(&tq->lock);
    addr = queue_pop(&tq->queue);
    pthread_rwlock_unlock(&tq->lock);

    return addr;
}

/******************************************************************************
 **函数名称: crwl_task_queue_destroy
 **功    能: 销毁任务队列
 **输入参数: 
 **     tq: 任务队列
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.10.12 #
 ******************************************************************************/
void crwl_task_queue_destroy(crwl_task_queue_t *tq)
{
    pthread_rwlock_destroy(&tq->lock);
    queue_destroy(&tq->queue);
}

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
    int ret, fd;
    uri_field_t field;
    struct hostent *host;
    time_t ctm = time(NULL);
    crwl_worker_socket_t *sck;
    char ipaddr[IP_ADDR_MAX_LEN];

    memset(&field, 0, sizeof(field));

    /* 解析URI字串 */
    if(uri_reslove(args->uri, &field))
    {
        log_error(worker->log, "Reslove uri [%d] failed!", args->uri);
        return CRWL_ERR;
    }
   
    /* 1. 通过URL获取WEB服务器信息 */
    host = gethostbyname(field.host);
    if (NULL == host)
    {
        log_error(worker->log, "Get host by name failed! uri:%s", args->uri);
        return CRWL_OK;
    }

    inet_ntop(AF_INET, host->h_addr, ipaddr, sizeof(ipaddr));

    /* 2. 连接远程WEB服务器 */
    fd = tcp_connect_ex2(ipaddr, args->port);
    if (fd < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s! ipaddr:%s uri:%s",
            errno, strerror(errno), ipaddr, args->uri);
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
    sck->crtm = ctm;
    sck->rdtm = ctm;
    sck->wrtm = ctm;
    sck->read.addr = sck->recv;

    snprintf(sck->webpage.uri, sizeof(sck->webpage.uri), "%s", args->uri);
    snprintf(sck->webpage.ipaddr, sizeof(sck->webpage.ipaddr), "%s", ipaddr);
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
        eslab_dealloc(&worker->slab, sck);
        return CRWL_ERR;
    }

    /* 5. 新建存储文件 */
    ret = crwl_worker_webpage_creat(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Save webpage failed!");

        crwl_worker_remove_sock(worker, sck);
        eslab_dealloc(&worker->slab, sck);
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
