#include "sdtp_cmd.h"
#include "sdtp_recv.h"
#include "sdtp_comm.h"

/******************************************************************************
 **函数名称: sdtp_cmd_to_rsvr
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
int sdtp_cmd_to_rsvr(sdtp_rctx_t *ctx, int cmd_sck_id, const sdtp_cmd_t *cmd, int idx)
{
    char path[FILE_PATH_MAX_LEN];
    sdtp_conf_t *conf = &ctx->conf;

    sdtp_rsvr_usck_path(conf, path, idx);

    /* 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(sdtp_cmd_t)) < 0)
    {
        log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                errno, strerror(errno), path, cmd->type);
        return SDTP_ERR;
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_link_auth_check
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
int sdtp_link_auth_check(sdtp_rctx_t *ctx, sdtp_link_auth_req_t *link_auth_req)
{
    sdtp_conf_t *conf = &ctx->conf;

    if (0 != strcmp(link_auth_req->usr, conf->auth.usr)
        || 0 != strcmp(link_auth_req->passwd, conf->auth.passwd))
    {
        return SDTP_LINK_AUTH_FAIL;
    }

    return SDTP_LINK_AUTH_SUCC;
}

/******************************************************************************
 **函数名称: sdtp_sck_dev_map_init
 **功    能: 创建SCK与DEV的映射表
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 20:29:26 #
 ******************************************************************************/
int sdtp_sck_dev_map_init(sdtp_rctx_t *ctx)
{
    avl_opt_t opt;

    /* > 创建SCK -> DEV映射表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->sck2dev_map = avl_creat(&opt,
                (key_cb_t)avl_key_cb_int64,
                (avl_cmp_cb_t)avl_cmp_cb_int64);
    if (NULL == ctx->sck2dev_map)
    {
        log_error(ctx->log, "Create sck2dev map table failed!");
        return SDTP_ERR;
    }

    /* > 创建DEV -> SCK的映射表 */
    memset(&opt, 0, sizeof(opt));

    opt.pool = (void *)ctx->pool;
    opt.alloc = (mem_alloc_cb_t)slab_alloc;
    opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

    ctx->dev2sck_map = avl_creat(&opt,
                (key_cb_t)avl_key_cb_int32,
                (avl_cmp_cb_t)avl_cmp_cb_int32);
    if (NULL == ctx->dev2sck_map)
    {
        log_error(ctx->log, "Create dev2sck map table failed!");
        return SDTP_ERR;
    }

    /* > 初始化读写锁 */
    pthread_rwlock_init(&ctx->sck2dev_map_lock, NULL);
    pthread_rwlock_init(&ctx->dev2sck_map_lock, NULL);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_sck2dev_map_add
 **功    能: 添加SCK->DEV映射
 **输入参数:
 **     ctx: 全局对象
 **     sck_serial: 套接字序列号
 **     devid: 设备ID
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: TODO: 待注册DEVID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
static int sdtp_sck2dev_map_add(sdtp_rctx_t *ctx,
        uint64_t sck_serial, int devid, int rsvr_idx)
{
    sdtp_sck2dev_item_t *item;

    /* > 新增SCK->DEV映射 */
    item = slab_alloc(ctx->pool, sizeof(sdtp_sck2dev_item_t));
    if (NULL == item)
    {
        log_error(ctx->log, "Alloc memory from slab failed!");
        return SDTP_ERR;
    }

    item->sck_serial = sck_serial;
    item->devid = devid;
    item->rsvr_idx = rsvr_idx;

    pthread_rwlock_wrlock(&ctx->sck2dev_map_lock);

    if (avl_insert(ctx->sck2dev_map, &item->sck_serial, sizeof(item->sck_serial), (void *)item))
    {
        pthread_rwlock_unlock(&ctx->sck2dev_map_lock);
        log_error(ctx->log, "Insert into sck2dev table failed! serial:%ld devid:%d",
                sck_serial, devid);
        slab_dealloc(ctx->pool, item);
        return SDTP_ERR;
    }

    pthread_rwlock_unlock(&ctx->sck2dev_map_lock);

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_dev2sck_map_add
 **功    能: 添加DEV->SCK映射
 **输入参数:
 **     ctx: 全局对象
 **     devid: 设备ID
 **     sck_serial: 套接字序列号
 **     rsvr_idx: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: TODO: 待注册DEVID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
static int sdtp_dev2sck_map_add(sdtp_rctx_t *ctx,
        int devid, uint64_t sck_serial, int rsvr_idx)
{
    list_t *list;
    list_opt_t opt;
    avl_node_t *avl_node;
    list_node_t *list_node;
    sdtp_dev2sck_item_t *item;

    pthread_rwlock_wrlock(&ctx->dev2sck_map_lock); /* 加锁 */

    while (1)
    {
        /* > 查找是否已经存在 */
        avl_node = avl_query(ctx->dev2sck_map, &devid, sizeof(devid));
        if (NULL != avl_node)
        {
            /* > 插入DEV -> SCK列表 */
            list = (list_t *)avl_node->data;
            list_node = list->head;
            for (; NULL != list_node; list_node = list_node->next)
            {
                item = (sdtp_dev2sck_item_t *)list_node->data;
                if (sck_serial == item->sck_serial) /* 判断是否重复 */
                {
                    pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */
                    log_error(ctx->log, "Socket serial repeat! serial:%ld", sck_serial);
                    return SDTP_OK;
                }
            }

            item = slab_alloc(ctx->pool, sizeof(sdtp_dev2sck_item_t));
            if (NULL == item)
            {
                pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */
                log_error(ctx->log, "Alloc memory failed! devid:%d serial:%ld",
                        devid, sck_serial);
                return SDTP_ERR;
            }

            item->sck_serial = sck_serial;
            item->rsvr_idx = rsvr_idx;

            if (list_lpush(list, item))
            {
                pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */
                log_error(ctx->log, "Alloc memory failed! devid:%d serial:%ld",
                        devid, sck_serial);
                slab_dealloc(ctx->pool, item);
                return SDTP_ERR;
            }

            pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */

            return SDTP_OK;
        }

        /* > 构建链表对象 */
        memset(&opt, 0, sizeof(opt));

        opt.pool = (void *)ctx->pool;
        opt.alloc = (mem_alloc_cb_t)slab_alloc;
        opt.dealloc = (mem_dealloc_cb_t)slab_dealloc;

        list = list_creat(&opt);
        if (NULL == list)
        {
            return SDTP_ERR;
        }

        if (avl_insert(ctx->dev2sck_map, &devid, sizeof(devid), (void *)list))
        {
            pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */
            log_error(ctx->log, "Insert into dev2sck table failed! devid:%d sck_serial:%ld",
                    devid, sck_serial);
            list_destroy(list, NULL, NULL);
            return SDTP_ERR;
        }
    }

    pthread_rwlock_unlock(&ctx->dev2sck_map_lock); /* 解锁 */

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_dev2sck_map_del
 **功    能: 删除DEV -> SCK映射
 **输入参数:
 **     ctx: 全局对象
 **     devid: 设备ID
 **     sck_serial: 需要删除的序列号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从链表中找出sck_serial结点, 并删除!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
static int sdtp_dev2sck_map_del(sdtp_rctx_t *ctx, int devid, uint64_t sck_serial)
{
    list_t *list;
    list_node_t *node;
    avl_node_t *avl_node;
    sdtp_dev2sck_item_t *item;

    /* > 获取链表对象 */
    avl_node = avl_query(ctx->dev2sck_map, &devid, sizeof(devid));
    if (NULL == avl_node)
    {
        log_error(ctx->log, "Query devid [%d] failed!", devid);
        return SDTP_ERR;
    }

    /* > 遍历链表查找sck_serial结点 */
    list = (list_t *)avl_node->data;
    if (NULL == list)
    {
        return SDTP_OK;
    }

    node = list->head;
    for (; NULL != node; node = node->next)
    {
        item = (sdtp_dev2sck_item_t *)node->data;
        if (item->sck_serial == sck_serial)
        {
            list_remove(list, item);
            slab_dealloc(ctx->pool, item);
            log_debug(ctx->log, "Delete dev2sck success! devid:%d sck_serial:%ld",
                    devid, sck_serial);
            return SDTP_OK;
        }
    }

    return SDTP_OK;
}

/******************************************************************************
 **函数名称: sdtp_sck_dev_map_add
 **功    能: 添加SCK与DEV的映射
 **输入参数:
 **     ctx: 全局对象
 **     rsvr_idx: 接收线程ID
 **     sck: 套接字对象
 **     link_auth_req: 鉴权请求
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: TODO: 待注册DEVID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
int sdtp_sck_dev_map_add(sdtp_rctx_t *ctx,
        int rsvr_idx, sdtp_rsck_t *sck, sdtp_link_auth_req_t *link_auth_req)
{
    if (sdtp_sck2dev_map_add(ctx, sck->serial, link_auth_req->devid, rsvr_idx))
    {
        log_error(ctx->log, "Add into sck2dev map failed!");
        return SDTP_ERR;
    }

    /* > 新增DEV->SCK映射 */
    return sdtp_dev2sck_map_add(ctx, link_auth_req->devid, sck->serial, rsvr_idx);
}

/******************************************************************************
 **函数名称: sdtp_sck_dev_map_del
 **功    能: 删除SCK与DEV的映射
 **输入参数:
 **     ctx: 全局对象
 **     sck_serial: 套接字序列
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
int sdtp_sck_dev_map_del(sdtp_rctx_t *ctx, uint64_t sck_serial)
{
    int devid = -1;
    sdtp_sck2dev_item_t *item;

    /* > 释放SCK->DEV映射 */
    pthread_rwlock_wrlock(&ctx->sck2dev_map_lock);
    avl_delete(ctx->sck2dev_map, &sck_serial, sizeof(sck_serial), (void **)&item);
    pthread_rwlock_unlock(&ctx->sck2dev_map_lock);
    if (NULL != item)
    {
        devid = item->devid;
        slab_dealloc(ctx->pool, item);
    }

    /* > 释放DEV->SCK映射 */
    return sdtp_dev2sck_map_del(ctx, devid, sck_serial);
}
