#include "redo.h"
#include "rtmq_comm.h"
#include "rtmq_recv.h"

/******************************************************************************
 **函数名称: rtmq_cmd_to_rsvr
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
int rtmq_cmd_to_rsvr(rtmq_cntx_t *ctx, int cmd_sck_id, const rtmq_cmd_t *cmd, int idx)
{
    char path[FILE_PATH_MAX_LEN];

    rtmq_rsvr_usck_path(&ctx->conf, path, idx);

    /* 发送命令至接收线程 */
    if (unix_udp_send(cmd_sck_id, path, cmd, sizeof(rtmq_cmd_t)) < 0) {
        if (EAGAIN != errno) {
            log_error(ctx->log, "errmsg:[%d] %s! path:%s type:%d",
                      errno, strerror(errno), path, cmd->type);
        }
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_link_auth_check
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
int rtmq_link_auth_check(rtmq_cntx_t *ctx, rtmq_link_auth_req_t *link_auth_req)
{
    return rtmq_auth_check(ctx,
            link_auth_req->usr,
            link_auth_req->passwd)?  RTMQ_LINK_AUTH_SUCC : RTMQ_LINK_AUTH_FAIL;
}

/******************************************************************************
 **函数名称: rtmq_node_to_svr_map_init
 **功    能: 创建NODE与SVR的映射表
 **输入参数:
 **     ctx: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 构建平衡二叉树
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 20:29:26 #
 ******************************************************************************/
static int rtmq_node_to_svr_map_cmp_cb(
        const rtmq_node_to_svr_map_t *map1, const rtmq_node_to_svr_map_t *map2)
{
    return (map1->nid - map2->nid);
}

int rtmq_node_to_svr_map_init(rtmq_cntx_t *ctx)
{
    /* > 创建映射表 */
    ctx->node_to_svr_map = avl_creat(NULL, (cmp_cb_t)rtmq_node_to_svr_map_cmp_cb);
    if (NULL == ctx->node_to_svr_map) {
        log_error(ctx->log, "Initialize dev->svr map failed!");
        return RTMQ_ERR;
    }

    /* > 初始化读写锁 */
    pthread_rwlock_init(&ctx->node_to_svr_map_lock, NULL);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_node_to_svr_map_add
 **功    能: 添加NODE->SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nid: 结点ID(主键)
 **     rsvr_id: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 注册NODEID与RSVR的映射关系, 为自定义数据的应答做铺垫!
 **作    者: # Qifeng.zou # 2015.05.30 #
 ******************************************************************************/
int rtmq_node_to_svr_map_add(rtmq_cntx_t *ctx, int nid, int rsvr_id)
{
    rtmq_node_to_svr_map_t *map, key;

    key.nid = nid;

    pthread_rwlock_wrlock(&ctx->node_to_svr_map_lock); /* 加锁 */

    /* > 查找是否已经存在 */
    map = avl_query(ctx->node_to_svr_map, &key);
    if (NULL == map) {
        map = (rtmq_node_to_svr_map_t *)calloc(1, sizeof(rtmq_node_to_svr_map_t));
        if (NULL == map) {
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
            log_error(ctx->log, "Alloc memory failed!");
            return RTMQ_ERR;
        }

        map->num = 0;
        map->nid = nid;

        if (avl_insert(ctx->node_to_svr_map, (void *)map)) {
            pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
            FREE(map);
            log_error(ctx->log, "Insert into dev2sck table failed! nid:%d rsvr_id:%d",
                      nid, rsvr_id);
            return RTMQ_ERR;
        }
    }

    /* > 插入NODE -> SVR列表 */
    if (map->num >= RTRD_NODE_TO_SVR_MAX_LEN) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */
        log_error(ctx->log, "Node to svr map is full! nid:%d", nid);
        return RTMQ_ERR;
    }

    map->rsvr_id[map->num++] = rsvr_id; /* 插入 */

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock); /* 解锁 */

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_node_to_svr_map_del
 **功    能: 删除NODE -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nid: 结点ID
 **     rsvr_id: 接收服务索引
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 从链表中找出sck_serial结点, 并删除!
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int rtmq_node_to_svr_map_del(rtmq_cntx_t *ctx, int nid, int rsvr_id)
{
    int idx;
    rtmq_node_to_svr_map_t *map, key;

    key.nid = nid;

    pthread_rwlock_wrlock(&ctx->node_to_svr_map_lock);

    /* > 查找映射表 */
    map = avl_query(ctx->node_to_svr_map, &key);
    if (NULL == map) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
        log_error(ctx->log, "Query nid [%d] failed!", nid);
        return RTMQ_ERR;
    }

    /* > 删除处理 */
    for (idx=0; idx<map->num; ++idx) {
        if (map->rsvr_id[idx] == rsvr_id) {
            map->rsvr_id[idx] = map->rsvr_id[--map->num]; /* 删除:使用最后一个值替代当前值 */
            if (0 == map->num) {
                avl_delete(ctx->node_to_svr_map, &key, (void *)&map);
                FREE(map);
            }
            break;
        }
    }

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_node_to_svr_map_rand
 **功    能: 随机选择NODE -> SVR映射
 **输入参数:
 **     ctx: 全局对象
 **     nid: 结点ID
 **输出参数: NONE
 **返    回: 接收线程索引
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.30 22:25:20 #
 ******************************************************************************/
int rtmq_node_to_svr_map_rand(rtmq_cntx_t *ctx, int nid)
{
    int rsvr_id;
    rtmq_node_to_svr_map_t *map, key;

    key.nid = nid;

    pthread_rwlock_rdlock(&ctx->node_to_svr_map_lock);

    /* > 获取映射表 */
    map = avl_query(ctx->node_to_svr_map, &key);
    if (NULL == map) {
        pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);
        log_error(ctx->log, "Query nid [%d] failed!", nid);
        return -1;
    }

    /* > 选择服务ID */
    rsvr_id = map->rsvr_id[rand() % map->num]; /* 随机选择 */

    pthread_rwlock_unlock(&ctx->node_to_svr_map_lock);

    return rsvr_id;
}

/* 订阅哈希回调 */
static uint64_t rtmq_sub_tab_hash_cb(const rtmq_sub_list_t *list)
{
    return list->type;
}

/* 订阅比较回调 */
static int rtmq_sub_tab_cmp_cb(const rtmq_sub_list_t *list1, const rtmq_sub_list_t *list2)
{
    return (list1->type - list2->type);
}

int rtmq_sub_mgr_init(rtmq_sub_mgr_t *sub)
{
    /* > 创建ONE订阅表 */
    sub->sub_one_tab = hash_tab_creat(100,
            (hash_cb_t)rtmq_sub_tab_hash_cb,
            (cmp_cb_t)rtmq_sub_tab_cmp_cb, NULL);
    if (NULL == sub->sub_one_tab) {
        return RTMQ_ERR;
    }

    /* > 创建ALL订阅表 */
    sub->sub_all_tab = hash_tab_creat(100,
            (hash_cb_t)rtmq_sub_tab_hash_cb,
            (cmp_cb_t)rtmq_sub_tab_cmp_cb, NULL);
    if (NULL == sub->sub_all_tab) {
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_sub_query
 **功    能: 获取订阅type的结点ID
 **输入参数:
 **     type: Message type
 **输出参数: NONE
 **返    回: 结点ID(-1:无订阅结点)
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2016.04.23 13:29:24 #
 ******************************************************************************/
int rtmq_sub_query(rtmq_cntx_t *ctx, mesg_type_e type)
{
    int nid;
    rtmq_sub_node_t *node;
    rtmq_sub_list_t *list, key;
    rtmq_sub_mgr_t *sub = &ctx->sub_mgr;

    key.type = type;

    list = hash_tab_query(sub->sub_one_tab, &key, RDLOCK);
    if (NULL == list) {
        log_debug(ctx->log, "No module sub this type! type:%d", type);
        return -1;
    }
    else if (0 == list2_len(list->nodes)) {
        hash_tab_unlock(sub->sub_one_tab, &key, RDLOCK);
        log_debug(ctx->log, "No module sub this type! type:%d", type);
        return -1;
    }

    node = (rtmq_sub_node_t *)list2_roll(list->nodes);
    if (NULL == node) {
        hash_tab_unlock(sub->sub_one_tab, &key, RDLOCK);
        log_debug(ctx->log, "Get sub node failed! type:%d", type);
        return -1;
    }

    nid = node->nid;
    hash_tab_unlock(sub->sub_one_tab, &key, RDLOCK);

    log_debug(ctx->log, "Node [%d] has sub type [%d]!", nid, type);

    return nid;
}
