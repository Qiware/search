#include "redo.h"
#include "sdtp_cmd.h"
#include "sdtp_comm.h"
#include "sdrd_recv.h"

/******************************************************************************
 **函数名称: sdrd_cmd_to_rsvr
 **功    能: 发送命令到指定的接收线程
 **输入参数:
 **     ctx: 全局对象
 **     cmd_sck_id: 命令套接字
 **     cmd: 处理命令
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 随机选择接收线程
 **     2. 发送命令至接收线程
 **注意事项: 如果发送失败，最多重复3次发送!
 **作    者: # Qifeng.zou # 2015.01.09 #
 ******************************************************************************/
int sdrd_cmd_to_rsvr(sdrd_cntx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd, int idx)
{
    char path[FILE_PATH_MAX_LEN];
    sdrd_conf_t *conf = &ctx->conf;

    sdrd_rsvr_usck_path(conf, path, idx);

    /* 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(sdtp_cmd_t)) < 0) {
        if (EAGAIN != errno) {
            log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                      errno, strerror(errno), path, cmd->type);
        }
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdrd_link_auth_check
 **功    能: 链路鉴权检测
 **输入参数:
 **     ctx: 全局对象
 **     link_auth_req: 鉴权请求
 **输出参数: NONE
 **返    回: succ:成功 fail:失败
 **实现描述: 检测用户名和密码是否正确
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.22 #
 ******************************************************************************/
int sdrd_link_auth_check(sdrd_cntx_t *ctx, sdtp_link_auth_req_t *link_auth_req)
{
    sdrd_conf_t *conf = &ctx->conf;

    if (0 != strcmp(link_auth_req->usr, conf->auth.usr)
        || 0 != strcmp(link_auth_req->passwd, conf->auth.passwd))
    {
        return SDTP_LINK_AUTH_FAIL;
    }

    return SDTP_LINK_AUTH_SUCC;
}

/******************************************************************************
 **函数名称: sdrd_node_to_svr_map_init
 **功    能: 创建NODE与SVR的映射表
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 20:29:26 #
 ******************************************************************************/
int sdrd_node_to_svr_map_init(sdrd_cntx_t *ctx)
{
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)NULL;
    opt.alloc = (mem_alloc_cb_t)mem_alloc;
    opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

    ctx->node_to_svr_map = avl_creat(&opt,
                (key_cb_t)key_cb_int32,
                (cmp_cb_t)cmp_cb_int32);
    if (NULL == ctx->node_to_svr_map) {
        log_error(ctx->log, "Initialize dev->svr map failed!");
        return SDTP_ERR;
    }

    /* > 初始化读写锁 */
    pthread_rwlock_init(&ctx->node_to_svr_map_lock, NULL);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdrd_node_to_svr_map_add
 **功    能: 添加NODE->SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nodeid: 结点ID(主键)
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 注册NODEID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
int sdrd_node_to_svr_map_add(sdrd_cntx_t *ctx, int nodeid, int rsvr_idx)
{
    list_t *list;
    list_opt_t opt;
    list_node_t *list_node;
    sdrd_node_to_svr_item_t *item;

    pthread_rwlock_wrlock(&ctx->node_to_svr_map_lock); /* 加锁 */

    while (1) {
        /* > 查找是否已经存在 */
        list = avl_query(ctx->node_to_svr_map, &nodeid, sizeof(nodeid));
        if (NULL == list) {
            /* > 构建链表对象 */
            memset(&opt, 0, sizeof(opt));

            opt.pool = (void *)NULL;
            opt.alloc = (mem_alloc_cb_t)mem_alloc;
            opt.dealloc = (mem_dealloc_cb_t)mem_dealloc;

            list = list_creat(&opt);
            if (NULL == list) {
                return SDTP_ERR;
            }

            if (avl_insert(ctx->node_to_svr_map, &nodeid, sizeof(nodeid), (void *)list)) {
                pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
                log_error(ctx->log, "Insert into dev2sck table failed! nodeid:%d rsvr_idx:%d",
                        nodeid, rsvr_idx);
                list_destroy(list, NULL, NULL);
                return SDTP_ERR;
            }
            continue;
        }

        /* > 插入NODE -> SVR列表 */
        list_node = list->head;
        for (; NULL != list_node; list_node = list_node->next) {
            item = (sdrd_node_to_svr_item_t *)list_node->data;
            if (rsvr_idx == item->rsvr_idx) { /* 判断是否重复 */
                ++item->count;
                pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
                return SDTP_OK;
            }
        }

        item = calloc(1, sizeof(sdrd_node_to_svr_item_t));
        if (NULL == item) {
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
            log_error(ctx->log, "Alloc memory failed! nodeid:%d rsvr_idx:%d",
                    nodeid, rsvr_idx);
            return SDTP_ERR;
        }

        item->rsvr_idx = rsvr_idx;
        item->count = 1;

        if (list_lpush(list, item)) {
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
            log_error(ctx->log, "Alloc memory failed! nodeid:%d rsvr_idx:%d",
                    nodeid, rsvr_idx);
            FREE(item);
            return SDTP_ERR;
        }

        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */

        return SDTP_OK;
    }

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdrd_node_to_svr_map_del
 **功    能: 删除NODE -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nodeid: 结点ID
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从链表中找出sck_serial结点, 并删除!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int sdrd_node_to_svr_map_del(sdrd_cntx_t *ctx, int nodeid, int rsvr_idx)
{
    list_t *list;
    list_node_t *node;
    sdrd_node_to_svr_item_t *item;

    pthread_rwlock_wrlock(&ctx->node_to_svr_map_lock);

    /* > 获取链表对象 */
    list = avl_query(ctx->node_to_svr_map, &nodeid, sizeof(nodeid));
    if (NULL == list) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
        log_error(ctx->log, "Query nodeid [%d] failed!", nodeid);
        return SDTP_ERR;
    }

    node = list->head;
    for (; NULL != node; node = node->next) {
        item = (sdrd_node_to_svr_item_t *)node->data;
        if (item->rsvr_idx == rsvr_idx) {
            --item->count;
            if (0 == item->count) {
                list_remove(list, item);
            }
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
            FREE(item);
            log_debug(ctx->log, "Delete dev svr map success! nodeid:%d rsvr_idx:%d",
                    nodeid, rsvr_idx);
            return SDTP_OK;
        }
    }

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdrd_node_to_svr_map_rand
 **功    能: 随机选择NODE -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nodeid: 结点ID
 **输出参数: NONE
 **返    回: 接收线程索引
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int sdrd_node_to_svr_map_rand(sdrd_cntx_t *ctx, int nodeid)
{
    int idx, n, rsvr_idx;
    list_t *list;
    list_node_t *node;
    sdrd_node_to_svr_item_t *item;

    pthread_rwlock_rdlock(&ctx->node_to_svr_map_lock);

    /* > 获取链表对象 */
    list = avl_query(ctx->node_to_svr_map, &nodeid, sizeof(nodeid));
    if (NULL == list) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
        log_error(ctx->log, "Query nodeid [%d] failed!", nodeid);
        return -1;
    }
    else if (0 == list->num) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
        return -1;
    }

    /* > 遍历链表查找sck_serial结点 */
    idx = rand() % list->num; /* 随机选择 */
    node = list->head;
    for (n = 0; NULL != node; node = node->next, ++n) {
        item = (sdrd_node_to_svr_item_t *)node->data;
        if (n == idx) {
            rsvr_idx = item->rsvr_idx;
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
            return rsvr_idx;
        }
    }

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
    return -1;
}

/******************************************************************************
 **函数名称: sdrd_shm_distq_creat
 **功    能: 创建SHM发送队列
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 通过路径生成KEY，再根据KEY创建共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.20 #
 ******************************************************************************/
shm_queue_t *sdrd_shm_distq_creat(const sdrd_conf_t *conf)
{
    char path[FILE_NAME_MAX_LEN];

    /* > 获取KEY路径 */
    sdrd_shm_distq_path(conf, path);

    /* > 通过路径创建共享内存队列 */
    return shm_queue_creat(path, conf->sendq.max, conf->sendq.size);
}

/******************************************************************************
 **函数名称: sdrd_distq_attach
 **功    能: 附着SHM发送队列
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 通过路径生成KEY，再根据KEY附着共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
shm_queue_t *sdrd_distq_attach(const sdrd_conf_t *conf)
{
    char path[FILE_NAME_MAX_LEN];

    /* > 通过路径生成KEY */
    sdrd_shm_distq_path(conf, path);

    /* > 通过KEY创建共享内存队列 */
    return shm_queue_attach(path);
}

/******************************************************************************
 **函数名称: sdrd_dist_routine
 **功    能: 运行分发线程
 **输入参数:
 **     ctx: 全局信息
 **输出参数: NONE
 **返    回: 分发对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.15 #
 ******************************************************************************/
void *sdrd_dist_routine(void *_ctx)
{
    int idx;
    void *data, *addr;
    sdtp_frwd_t *frwd;
    sdrd_cntx_t *ctx = (sdrd_cntx_t *)_ctx;

    while (1) {
        /* > 弹出发送数据 */
        data = shm_queue_pop(ctx->distq);
        if (NULL == data) {
            usleep(500); /* TODO: 可使用事件通知机制减少CPU的消耗 */
            continue;
        }

        /* > 获取发送队列 */
        frwd = (sdtp_frwd_t *)data;

        idx = sdrd_node_to_svr_map_rand(ctx, frwd->dest);
        if (idx < 0) {
            log_error(ctx->log, "Didn't find dev to svr map! nodeid:%d", frwd->dest);
            continue;
        }

        /* > 获取发送队列 */
        addr = queue_malloc(ctx->sendq[idx], frwd->length);
        if (NULL == addr) {
            shm_queue_dealloc(ctx->distq, data);
            log_error(ctx->log, "Alloc from queue failed! size:%d/%d",
                    frwd->length, queue_size(ctx->sendq[idx]));
            continue;
        }

        memcpy(addr, data, frwd->length);

        queue_push(ctx->sendq[idx], addr);

        shm_queue_dealloc(ctx->distq, data);
    }

    return (void *)-1;
}
