#include <netdb.h>

#include "log.h"
#include "common.h"
#include "crawler.h"
#include "syscall.h"
#include "xdo_socket.h"
#include "crwl_worker.h"
#include "crwl_worker_task.h"

/******************************************************************************
 **函数名称: crwl_worker_task_load_url
 **功    能: 加载URL的任务处理
 **输入参数: 
 **     worker: 爬虫对象 
 **     url: URL
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.09.25 #
 ******************************************************************************/
int crwl_worker_task_load_url(crwl_worker_t *worker, const char *url)
{
    int ret, fd;
    struct hostent *host;
    crwl_worker_sck_t *sck;
    char ipaddr[IP_ADDR_MAX_LEN];
    
    /* 1. 通过URL获取WEB服务器信息 */
    host = gethostbyname(url);
    if (NULL == host)
    {
        log_error(worker->log, "Get host by name failed! url:%s", url);
        return CRWL_OK;
    }

    inet_ntop(AF_INET, host->h_addr, ipaddr, sizeof(ipaddr));

    /* 2. 连接远程WEB服务器 */
    fd = tcp_connect_ex(ipaddr, CRWL_WRK_WEB_SVR_PORT, CRWL_WRK_CONNECT_TMOUT);
    if (fd < 0)
    {
        log_error(worker->log, "errmsg:[%d] %s! ipaddr:%s url:%s",
            errno, strerror(errno), ipaddr, url);
        return CRWL_OK;
    }

    /* 3. 将FD等信息加入套接字链表 */
    sck = eslab_alloc(&worker->slab, sizeof(crwl_worker_sck_t));
    if (NULL == sck)
    {
        log_error(worker->log, "Alloc memory from slab failed!");
        return CRWL_ERR;
    }

    memset(sck, 0, sizeof(crwl_worker_sck_t));

    sck->sckid = fd;
    snprintf(sck->url, sizeof(sck->url), "%s", url);
    snprintf(sck->ipaddr, sizeof(sck->ipaddr), "%s", ipaddr);
    sck->port = CRWL_WRK_WEB_SVR_PORT;

    ret = crwl_worker_add_sck(worker, sck);
    if (CRWL_OK != ret)
    {
        log_error(worker->log, "Add socket into list failed!");
        eslab_free(&worker->slab, sck);
        return CRWL_ERR;
    }

    return CRWL_OK;
}
