#include "rttp_cmd.h"
#include "rttp_comm.h"
#include "rtrd_recv.h"

/******************************************************************************
 **函数名称: rtrd_cmd_to_rsvr
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
int rtrd_cmd_to_rsvr(rtrd_cntx_t *ctx, int cmd_sck_id, const rttp_cmd_t *cmd, int idx)
{
    char path[FILE_PATH_MAX_LEN];
    rtrd_conf_t *conf = &ctx->conf;

    rtrd_rsvr_usck_path(conf, path, idx);

    /* 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(rttp_cmd_t)) < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                errno, strerror(errno), path, cmd->type);
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_link_auth_check
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
int rtrd_link_auth_check(rtrd_cntx_t *ctx, rttp_link_auth_req_t *link_auth_req)
{
    rtrd_conf_t *conf = &ctx->conf;

    if (0 != strcmp(link_auth_req->usr, conf->auth.usr)
        || 0 != strcmp(link_auth_req->passwd, conf->auth.passwd))
    {
        return RTTP_LINK_AUTH_FAIL;
    }

    return RTTP_LINK_AUTH_SUCC;
}

/******************************************************************************
 **函数名称: rtrd_dev_to_svr_map_init
 **功    能: 创建DEV与SVR的映射表
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 20:29:26 #
 ******************************************************************************/
int rtrd_dev_to_svr_map_init(rtrd_cntx_t *ctx)
{
    avl_opt_t opt;

    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->dev_to_svr_map = avl_creat(&opt,
                (key_cb_t)avl_key_cb_int32,
                (avl_cmp_cb_t)avl_cmp_cb_int32);
    if (NULL == ctx->dev_to_svr_map)
    {
        log_error(ctx->log, "Initialize dev->svr map failed!");
        return RTTP_ERR;
    }

    /* > 初始化读写锁 */
    pthread_rwlock_init(&ctx->dev_to_svr_map_lock, NULL);

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_dev_to_svr_map_add
 **功    能: 添加DEV->SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     devid: 设备ID(主键)
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 注册DEVID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
int rtrd_dev_to_svr_map_add(rtrd_cntx_t *ctx, int devid, int rsvr_idx)
{
    list_t *list;
    list_opt_t opt;
    avl_node_t *avl_node;
    list_node_t *list_node;
    rtrd_dev_to_svr_item_t *item;

    pthread_rwlock_wrlock(&ctx->dev_to_svr_map_lock); /* 加锁 */

    while (1)
    {
        /* > 查找是否已经存在 */
        avl_node = avl_query(ctx->dev_to_svr_map, &devid, sizeof(devid));
        if (NULL == avl_node)
        {
            /* > 构建链表对象 */
            memset(&opt, 0, sizeof(opt));

            opt.pool = (void *)ctx->pool;
            opt.alloc = (mem_alloc_cb_t)slab_alloc;
            opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

            list = list_creat(&opt);
            if (NULL == list)
            {
                return RTTP_ERR;
            }

            if (avl_insert(ctx->dev_to_svr_map, &devid, sizeof(devid), (void *)list))
            {
                pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */
                log_error(ctx->log, "Insert into dev2sck table failed! devid:%d rsvr_idx:%d",
                        devid, rsvr_idx);
                list_destroy(list, NULL, NULL);
                return RTTP_ERR;
            }

            continue;
        }

        /* > 插入DEV -> SVR列表 */
        list = (list_t *)avl_node->data;
        list_node = list->head;
        for (; NULL != list_node; list_node = list_node->next)
        {
            item = (rtrd_dev_to_svr_item_t *)list_node->data;
            if (rsvr_idx == item->rsvr_idx) /* 判断是否重复 */
            {
                ++item->count;
                pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */
                return RTTP_OK;
            }
        }

        item = slab_alloc(ctx->pool, sizeof(rtrd_dev_to_svr_item_t));
        if (NULL == item)
        {
            pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */
            log_error(ctx->log, "Alloc memory failed! devid:%d rsvr_idx:%d",
                    devid, rsvr_idx);
            return RTTP_ERR;
        }

        item->rsvr_idx = rsvr_idx;
        item->count = 1;

        if (list_lpush(list, item))
        {
            pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */
            log_error(ctx->log, "Alloc memory failed! devid:%d rsvr_idx:%d",
                    devid, rsvr_idx);
            slab_dealloc(ctx->pool, item);
            return RTTP_ERR;
        }

        pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */

        return RTTP_OK;
    }

    pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock); /* 解锁 */

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_dev_to_svr_map_del
 **功    能: 删除DEV -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     devid: 设备ID
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从链表中找出sck_serial结点, 并删除!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int rtrd_dev_to_svr_map_del(rtrd_cntx_t *ctx, int devid, int rsvr_idx)
{
    list_t *list;
    list_node_t *node;
    avl_node_t *avl_node;
    rtrd_dev_to_svr_item_t *item;

    pthread_rwlock_wrlock(&ctx->dev_to_svr_map_lock);

    /* > 获取链表对象 */
    avl_node = avl_query(ctx->dev_to_svr_map, &devid, sizeof(devid));
    if (NULL == avl_node)
    {
        pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
        log_error(ctx->log, "Query devid [%d] failed!", devid);
        return RTTP_ERR;
    }

    /* > 遍历链表查找sck_serial结点 */
    list = (list_t *)avl_node->data;
    if (NULL == list)
    {
        pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
        return RTTP_OK;
    }

    node = list->head;
    for (; NULL != node; node = node->next)
    {
        item = (rtrd_dev_to_svr_item_t *)node->data;
        if (item->rsvr_idx == rsvr_idx)
        {
            --item->count;
            if (0 == item->count)
            {
                list_remove(list, item);
            }
            pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
            slab_dealloc(ctx->pool, item);
            log_debug(ctx->log, "Delete dev svr map success! devid:%d rsvr_idx:%d",
                    devid, rsvr_idx);
            return RTTP_OK;
        }
    }

    pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtrd_dev_to_svr_map_rand
 **功    能: 随机选择DEV -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     devid: 设备ID
 **输出参数: NONE
 **返    回: 接收线程索引
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int rtrd_dev_to_svr_map_rand(rtrd_cntx_t *ctx, int devid)
{
    int idx, n, rsvr_idx;
    list_t *list;
    list_node_t *node;
    avl_node_t *avl_node;
    rtrd_dev_to_svr_item_t *item;

    pthread_rwlock_rdlock(&ctx->dev_to_svr_map_lock);

    /* > 获取链表对象 */
    avl_node = avl_query(ctx->dev_to_svr_map, &devid, sizeof(devid));
    if (NULL == avl_node)
    {
        pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
        log_error(ctx->log, "Query devid [%d] failed!", devid);
        return -1;
    }

    /* > 遍历链表查找sck_serial结点 */
    list = (list_t *)avl_node->data;
    if (NULL == list || 0 == list->num)
    {
        pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
        return -1;
    }

    idx = rand() % list->num; /* 随机选择 */
    node = list->head;
    for (n = 0; NULL != node; node = node->next, ++n)
    {
        item = (rtrd_dev_to_svr_item_t *)node->data;
        if (n == idx)
        {
            rsvr_idx = item->rsvr_idx;
            pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
            return rsvr_idx;
        }
    }

    pthread_rwlock_unlock(&ctx->dev_to_svr_map_lock);
    return -1;
}

/******************************************************************************
 **函数名称: rtrd_shm_sendq_creat
 **功    能: 创建SHM发送队列
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 通过路径生成KEY，再根据KEY创建共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.20 #
 ******************************************************************************/
shm_queue_t *rtrd_shm_sendq_creat(const rtrd_conf_t *conf)
{
    char path[FILE_NAME_MAX_LEN];

    /* > 获取KEY路径 */
    rtrd_shm_sendq_path(conf, path);

    /* > 通过路径创建共享内存队列 */
    return shm_queue_creat(path, conf->sendq.max, conf->sendq.size);
}

/******************************************************************************
 **函数名称: rtrd_shm_sendq_attach
 **功    能: 附着SHM发送队列
 **输入参数:
 **     conf: 配置信息
 **输出参数: NONE
 **返    回: 共享内存队列
 **实现描述: 通过路径生成KEY，再根据KEY附着共享内存队列
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.01 #
 ******************************************************************************/
shm_queue_t *rtrd_shm_sendq_attach(const rtrd_conf_t *conf)
{
    char path[FILE_NAME_MAX_LEN];

    /* > 通过路径生成KEY */
    rtrd_shm_sendq_path(conf, path);

    /* > 通过KEY创建共享内存队列 */
    return shm_queue_attach(path);
}
