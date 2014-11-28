#include "search.h"
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "hash.h"
#include "common.h"
#include "syscall.h"
#include "xml_tree.h"
#include "srch_recver.h"
#include "xd_socket.h"
#include "thread_pool.h"

static srch_recver_t *srch_recver_get(srch_cntx_t *ctx);
static int srch_recver_add_conn(srch_cntx_t *ctx, srch_recver_t *r);
static int srch_recver_socket_cmp_cb(const void *pkey, const void *data);

static int srch_recver_event_hdl(srch_cntx_t *ctx, srch_recver_t *r);
static int srch_recver_timeout_hdl(srch_recver_t *r);

/******************************************************************************
 **函数名称: srch_recver_routine
 **功    能: 运行接收线程
 **输入参数:
 **     _ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
void *srch_recver_routine(void *_ctx)
{
    srch_recver_t *r;
    srch_cntx_t *ctx = (srch_cntx_t *)_ctx;

    /* 1. 获取Recver对象 */
    r = srch_recver_get(ctx);
    if (NULL == r)
    {
        log_error(r->log, "Get recver failed!");
        pthread_exit((void *)-1);
        return (void *)-1;
    }

    while (1)
    {
        /* 2. 从连接队列取数据 */
        if (1)
        {
            if (srch_recver_add_conn(ctx, r))
            {
                log_error(r->log, "Fetch new connection failed!");
            }
        }

        /* 3. 等待事件通知 */
        r->fds = epoll_wait(r->ep_fd, r->events, SRCH_EVENT_MAX_NUM, SRCH_TMOUT_SEC);
        if (r->fds < 0)
        {
            if (EINTR == errno)
            {
                continue;
            }

            log_error(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
            abort();
            return (void *)-1;
        }
        else if (0 == r->fds)
        {
            /* 超时处理 */
            srch_recver_timeout_hdl(r);
            continue;
        }

        /* 3. 处理事件通知 */
        srch_recver_event_hdl(ctx, r);
    }

    return NULL;
}

/******************************************************************************
 **函数名称: srch_recver_init
 **功    能: 初始化接收线程
 **输入参数:
 **     ctx: 全局信息
 **     r: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     1. 创建epoll对象
 **     2. 创建命令套接字
 **     3. 创建套接字队列
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
int srch_recver_init(srch_cntx_t *ctx, srch_recver_t *r)
{
    void *addr;
    size_t size;
    char path[FILE_NAME_MAX_LEN];

    r->ctx = ctx;
    r->log = ctx->log;
    r->conf = ctx->conf;

    /* 1. 创建SLAB内存池 */
    addr = calloc(1, SRCH_SLAB_SIZE);
    if (NULL == addr)
    {
        log_fatal(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SRCH_ERR;
    }

    r->slab = slab_init(addr, SRCH_SLAB_SIZE);
    if (NULL == r->slab)
    {
        free(addr);
        log_error(r->log, "Initialize slab pool failed!");
        return SRCH_ERR;
    }

    /* 2. 创建套接字管理表 */
    r->sock_tab = hash_tab_creat(1000,
            hash_time33_ex,
            srch_recver_socket_cmp_cb);
    if (NULL == r->sock_tab)
    {
        return SRCH_ERR;
    }

    do
    {
        /* 2. 创建epoll对象 */
        r->ep_fd = epoll_create(SRCH_EVENT_MAX_NUM);
        if (r->ep_fd < 0)
        {
            log_error(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        r->events = slab_alloc(r->slab, SRCH_EVENT_MAX_NUM * sizeof(struct epoll_event));
        if (NULL == r->events)
        {
            log_error(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* 3. 创建命令套接字 */
        snprintf(path, sizeof(path), SRCH_RCV_CMD_PATH, r->tidx);

        r->cmd_sck_id = unix_udp_creat(path);
        if (r->cmd_sck_id < 0)
        {
            log_error(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* 4. 创建套接字队列 */
        size = 10000 * sizeof(srch_add_sck_t);

        r->conn_sckq = lqueue_init(10000, size);
        if (NULL == r->conn_sckq)
        {
            log_error(r->log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        return SRCH_OK;
    } while(0);

    slab_destroy(r->slab);
    Close(r->ep_fd);
    Close(r->cmd_sck_id);
    return SRCH_ERR;
}

/******************************************************************************
 **函数名称: srch_recver_destroy
 **功    能: 销毁接收线程
 **输入参数:
 **     recver: 接收对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.18 #
 ******************************************************************************/
int srch_recver_destroy(srch_recver_t *r)
{
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_get
 **功    能: 获取Recver对象
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: Recver对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.26 #
 ******************************************************************************/
static srch_recver_t *srch_recver_get(srch_cntx_t *ctx)
{
    int tidx;

    tidx = thread_pool_get_tidx(ctx->recvers);

    return (srch_recver_t *)ctx->recvers->data + tidx;
}

/******************************************************************************
 **函数名称: srch_recver_event_hdl
 **功    能: 事件通知处理
 **输入参数: 
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: Recver对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int srch_recver_event_hdl(srch_cntx_t *ctx, srch_recver_t *r)
{
    int idx;
    socket_t *sck;
    time_t ctm = time(NULL);

    /* 1. 依次遍历套接字, 判断是否可读可写 */
    for (idx=0; idx<r->fds; ++idx)
    {
        sck = (socket_t *)r->events[idx].data.ptr;

        /* 1.1 判断是否可读 */
        if (r->events[idx].events & EPOLLIN)
        {
            /* 接收网络数据 */
            if (sck->recv_cb(r, sck))
            {
                continue; /* 异常: 不必判断是否可写(套接字已关闭) */
            }
        }

        /* 1.2 判断是否可写 */
        if (r->events[idx].events & EPOLLOUT)
        {
            /* 发送网络数据 */
            if (sck->send_cb(r, sck))
            {
                continue; /* 异常: 套接字已关闭 */
            }
        }
    }

    /* 2. 超时扫描 */
    if (ctm - r->scan_tm > SRCH_TMOUT_SCAN_SEC)
    {
        r->scan_tm = ctm;

        srch_recver_timeout_hdl(r);
    }

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_timeout_hdl
 **功    能: 事件超时处理
 **输入参数: 
 **     ctx: 全局信息
 **     r: 
 **输出参数: NONE
 **返    回: Recver对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.28 #
 ******************************************************************************/
static int srch_recver_timeout_hdl(srch_recver_t *recver)
{
#if 0
    time_t ctm = time(NULL);
    list_node_t *node, *next;
    socket_t *sck;
    srch_recver_socket_data_t *data;

    /* 1. 依次遍历套接字, 判断是否超时 */
    node = recver->sock_list.head;
    for (; NULL != node; node = next)
    {
        next = node->next;

        sck = (socket_t *)node->data;
        if (NULL == sck)
        {
            continue;
        }

        data = (srch_recver_socket_data_t *)sck->data;

        /* 超时未发送或接收数据时, 认为无数据传输, 将直接关闭套接字 */
        if ((ctm - sck->rdtm <= 15)
            || (ctm - sck->wrtm <= 15))
        {
            continue; /* 未超时 */
        }

        log_error(recver->log, "Timeout!");

       srch_recver_remove_sock(recver, sck);
    }
#endif
    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_add_conn
 **功    能: 添加新的连接
 **输入参数: 
 **     ctx: 全局信息
 **     r: Recver对象
 **输出参数: NONE
 **返    回: Recver对象
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_recver_add_conn(srch_cntx_t *ctx, srch_recver_t *r)
{
    time_t ctm = time(NULL);
    socket_t *sck;
    srch_add_sck_t *add;
    srch_recver_socket_data_t *data;
    struct epoll_event ev;

    /* 1. 取数据 */
    add = lqueue_trypop(r->conn_sckq);
    if (NULL == add)
    {
        return SRCH_OK;
    }

    /* 2. 申请SCK空间 */
    sck = slab_alloc(r->slab, sizeof(socket_t));
    if (NULL == sck)
    {
        lqueue_mem_dealloc(r->conn_sckq, add);
        log_error(r->log, "Alloc memory from slab failed!");
        return SRCH_ERR;
    }

    data = slab_alloc(r->slab, sizeof(srch_recver_socket_data_t));
    if (NULL == data)
    {
        slab_dealloc(r->slab, sck);
        lqueue_mem_dealloc(r->conn_sckq, add);
        log_error(r->log, "Alloc memory from slab failed!");
        return SRCH_ERR;
    }

    sck->data = data;

    /* 3. 设置SCK信息 */
    sck->fd = add->fd;
    ftime(&sck->crtm);          /* 创建时间 */
    sck->wrtm = sck->rdtm = ctm;/* 记录当前时间 */

    data->sck_serial = add->sck_serial;

    lqueue_mem_dealloc(r->conn_sckq, add);

    /* 4. 哈希表中(以序列号为主键) */
    if (hash_tab_insert(r->sock_tab, &data->sck_serial, sizeof(data->sck_serial), sck))
    {
        Close(sck->fd);
        slab_dealloc(r->slab, sck->data);
        slab_dealloc(r->slab, sck);
        log_error(r->log, "Insert into hash table failed!");
        return SRCH_ERR;
    }

    /* 5. 加入epoll监听(首先是接收数据, 所以设置EPOLLIN) */
    memset(&ev, 0, sizeof(ev));

    ev.data.ptr = sck;
    ev.events = EPOLLIN | EPOLLET; /* 边缘触发 */

    epoll_ctl(r->ep_fd, EPOLL_CTL_ADD, sck->fd, &ev);

    return SRCH_OK;
}

/******************************************************************************
 **函数名称: srch_recver_socket_cmp_cb
 **功    能: 哈希表中查找套接字信息的比较回调函数
 **输入参数:
 **     pkey: 主键
 **     data: 哈希表树中结点所挂载的数据
 **输出参数: NONE
 **返    回:
 **     1. 0: 相等
 **     2. <0: 小于
 **     3. >0: 大于
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.29 #
 ******************************************************************************/
static int srch_recver_socket_cmp_cb(const void *pkey, const void *data)
{
    uint64_t sck_serial = *(const uint64_t *)pkey;
    const socket_t *sock = (const socket_t *)data;
    const srch_recver_socket_data_t *sock_data = sock->data;

    return (sck_serial - sock_data->sck_serial);
}
