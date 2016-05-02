#include "comm.h"
#include "lock.h"
#include "redo.h"
#include "syscall.h"
#include "rtmq_mesg.h"
#include "rtmq_proxy.h"

static int rtmq_proxy_creat_cmd_usck(rtmq_proxy_t *pxy);

/******************************************************************************
 **函数名称: rtmq_proxy_creat_workers
 **功    能: 创建工作线程线程池
 **输入参数:
 **     pxy: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtmq_proxy_creat_workers(rtmq_proxy_t *pxy)
{
    int idx;
    rtmq_worker_t *worker;
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 创建对象 */
    worker = (rtmq_worker_t *)calloc(conf->work_thd_num, sizeof(rtmq_worker_t));
    if (NULL == worker) {
        log_error(pxy->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 创建线程池 */
    pxy->worktp = thread_pool_init(conf->work_thd_num, NULL, (void *)worker);
    if (NULL == pxy->worktp) {
        log_error(pxy->log, "Initialize thread pool failed!");
        free(worker);
        return RTMQ_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->work_thd_num; ++idx) {
        if (rtmq_proxy_worker_init(pxy, worker+idx, idx)) {
            log_fatal(pxy->log, "Initialize work thread failed!");
            free(worker);
            thread_pool_destroy(pxy->worktp);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_creat_sends
 **功    能: 创建发送线程线程池
 **输入参数:
 **     pxy: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.08.19 #
 ******************************************************************************/
static int rtmq_proxy_creat_sends(rtmq_proxy_t *pxy)
{
    int idx;
    rtmq_proxy_ssvr_t *ssvr;
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 创建对象 */
    ssvr = (rtmq_proxy_ssvr_t *)calloc(conf->send_thd_num, sizeof(rtmq_proxy_ssvr_t));
    if (NULL == ssvr) {
        log_error(pxy->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 创建线程池 */
    pxy->sendtp = thread_pool_init(conf->send_thd_num, NULL, (void *)ssvr);
    if (NULL == pxy->sendtp) {
        log_error(pxy->log, "Initialize thread pool failed!");
        free(ssvr);
        return RTMQ_ERR;
    }

    /* > 初始化线程 */
    for (idx=0; idx<conf->send_thd_num; ++idx) {
        if (rtmq_proxy_ssvr_init(pxy, ssvr+idx, idx)) {
            log_fatal(pxy->log, "Initialize send thread failed!");
            free(ssvr);
            thread_pool_destroy(pxy->sendtp);
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_creat_recvq
 **功    能: 创建接收队列
 **输入参数:
 **     pxy: 全局对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.06.04 #
 ******************************************************************************/
static int rtmq_proxy_creat_recvq(rtmq_proxy_t *pxy)
{
    int idx;
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 创建队列对象 */
    pxy->recvq = (queue_t **)calloc(conf->send_thd_num, sizeof(queue_t *));
    if (NULL == pxy->recvq) {
        log_error(pxy->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 创建接收队列 */
    for (idx=0; idx<conf->work_thd_num; ++idx) {
        pxy->recvq[idx] = queue_creat(conf->recvq.max, conf->recvq.size);
        if (NULL == pxy->recvq[idx]) {
            log_error(pxy->log, "Create recvq failed!");
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_creat_sendq
 **功    能: 创建发送线程的发送队列
 **输入参数:
 **     pxy: 发送对象
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2016.01.01 22:32:21 #
 ******************************************************************************/
static int rtmq_proxy_creat_sendq(rtmq_proxy_t *pxy)
{
    int idx;
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 创建队列对象 */
    pxy->sendq = (queue_t **)calloc(conf->send_thd_num, sizeof(queue_t *));
    if (NULL == pxy->sendq) {
        log_error(pxy->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTMQ_ERR;
    }

    /* > 创建发送队列 */
    for (idx=0; idx<conf->send_thd_num; ++idx) {
        pxy->sendq[idx] = queue_creat(conf->sendq.max, conf->sendq.size);
        if (NULL == pxy->sendq[idx]) {
            log_error(pxy->log, "Create send queue failed!");
            return RTMQ_ERR;
        }
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_init
 **功    能: 初始化发送端
 **输入参数:
 **     conf: 配置信息
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 全局对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
rtmq_proxy_t *rtmq_proxy_init(const rtmq_proxy_conf_t *conf, log_cycle_t *log)
{
    int fd;
    rtmq_proxy_t *pxy;
    char path[FILE_NAME_MAX_LEN];

    /* > 创建对象 */
    pxy = (rtmq_proxy_t *)calloc(1, sizeof(rtmq_proxy_t));
    if (NULL == pxy) {
        log_fatal(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    pxy->log = log;

    /* > 加载配置信息 */
    memcpy(&pxy->conf, conf, sizeof(rtmq_proxy_conf_t));

    do {
        /* > 锁住指定文件(注: 防止路径和结点ID相同的配置) */
        rtmq_proxy_lock_path(conf, path);

        fd = Open(path, O_CREAT|O_RDWR|O_DIRECT, OPEN_MODE);
        if (fd < 0) {
            log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        if (proc_try_wrlock(fd)) {
            CLOSE(fd);
            log_error(log, "Lock failed! errmsg:[%d] %s!", errno, strerror(errno));
            break;
        }

        /* > 创建处理映射表 */
        pxy->reg = avl_creat(NULL, (key_cb_t)key_cb_int32, (cmp_cb_t)cmp_cb_int32);
        if (NULL == pxy->reg) {
            log_fatal(log, "Create register map failed!");
            break;
        }
        /* > 创建通信套接字 */
        if (rtmq_proxy_creat_cmd_usck(pxy)) {
            log_fatal(log, "Create cmd socket failed!");
            break;
        }

        /* > 创建接收队列 */
        if (rtmq_proxy_creat_recvq(pxy)) {
            log_fatal(log, "Create recv-queue failed!");
            break;
        }

        /* > 创建发送队列 */
        if (rtmq_proxy_creat_sendq(pxy)) {
            log_fatal(log, "Create send queue failed!");
            break;
        }

        /* > 创建工作线程池 */
        if (rtmq_proxy_creat_workers(pxy)) {
            log_fatal(pxy->log, "Create work thread pool failed!");
            break;
        }

        /* > 创建发送线程池 */
        if (rtmq_proxy_creat_sends(pxy)) {
            log_fatal(pxy->log, "Create send thread pool failed!");
            break;
        }

        return pxy;
    } while(0);

    free(pxy);
    return NULL;
}

/******************************************************************************
 **函数名称: rtmq_proxy_launch
 **功    能: 启动发送端
 **输入参数:
 **     pxy: 全局信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **     1. 创建工作线程池
 **     2. 创建发送线程池
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int rtmq_proxy_launch(rtmq_proxy_t *pxy)
{
    int idx;
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 注册Worker线程回调 */
    for (idx=0; idx<conf->work_thd_num; ++idx) {
        thread_pool_add_worker(pxy->worktp, rtmq_proxy_worker_routine, pxy);
    }

    /* > 注册Send线程回调 */
    for (idx=0; idx<conf->send_thd_num; ++idx) {
        thread_pool_add_worker(pxy->sendtp, rtmq_proxy_ssvr_routine, pxy);
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_register
 **功    能: 消息处理的注册接口
 **输入参数:
 **     pxy: 全局对象
 **     type: 扩展消息类型 Range:(0 ~ RTMQ_TYPE_MAX)
 **     proc: 回调函数
 **     param: 附加参数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **     1. 只能用于注册处理扩展数据类型的处理
 **     2. 不允许重复注册
 **作    者: # Qifeng.zou # 2015.05.19 #
 ******************************************************************************/
int rtmq_proxy_register(rtmq_proxy_t *pxy, int type, rtmq_reg_cb_t proc, void *param)
{
    rtmq_reg_t *item;

    item = (rtmq_reg_t *)calloc(1, sizeof(rtmq_reg_t));
    if (NULL == item) {
        log_error(pxy->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    item->type = type;
    item->proc = proc;
    item->param = param;

    if (avl_insert(pxy->reg, &type, sizeof(type), item)) {
        log_error(pxy->log, "Register maybe repeat! type:%d!", type);
        free(item);
        return RTMQ_ERR_REPEAT_REG;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_creat_cmd_usck
 **功    能: 创建命令套接字
 **输入参数:
 **     pxy: 上下文信息
 **     idx: 目标队列序号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtmq_proxy_creat_cmd_usck(rtmq_proxy_t *pxy)
{
    char path[FILE_NAME_MAX_LEN];

    rtmq_proxy_comm_usck_path(&pxy->conf, path);

    spin_lock_init(&pxy->cmd_sck_lck);
    pxy->cmd_sck_id = unix_udp_creat(path);
    if (pxy->cmd_sck_id < 0) {
        log_error(pxy->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return RTMQ_ERR;
    }

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_cli_cmd_send_req
 **功    能: 通知Send服务线程
 **输入参数:
 **     pxy: 上下文信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtmq_proxy_cli_cmd_send_req(rtmq_proxy_t *pxy, int idx)
{
    rtmq_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    rtmq_proxy_conf_t *conf = &pxy->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTMQ_CMD_SEND_ALL;
    rtmq_proxy_ssvr_usck_path(conf, path, idx);

    if (spin_trylock(&pxy->cmd_sck_lck)) {
        log_debug(pxy->log, "Try lock failed!");
        return RTMQ_OK;
    }

    if (unix_udp_send(pxy->cmd_sck_id, path, &cmd, sizeof(cmd)) < 0) {
        spin_unlock(&pxy->cmd_sck_lck);
        if (EAGAIN != errno) {
            log_debug(pxy->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        }
        return RTMQ_ERR;
    }

    spin_unlock(&pxy->cmd_sck_lck);

    return RTMQ_OK;
}

/******************************************************************************
 **函数名称: rtmq_proxy_async_send
 **功    能: 发送指定数据(对外接口)
 **输入参数:
 **     pxy: 上下文信息
 **     type: 数据类型
 **     nid: 源结点ID
 **     data: 数据地址
 **     size: 数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 将数据按照约定格式放入队列中
 **注意事项:
 **     1. 只能用于发送自定义数据类型, 而不能用于系统数据类型
 **     2. 不用关注变量num在多线程中的值, 因其不影响安全性
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int rtmq_proxy_async_send(rtmq_proxy_t *pxy, int type, const void *data, size_t size)
{
    int idx;
    void *addr;
    rtmq_header_t *head;
    static uint8_t num = 0; // 无需加锁
    rtmq_proxy_conf_t *conf = &pxy->conf;

    /* > 选择发送队列 */
    idx = (num++) % conf->send_thd_num;

    addr = queue_malloc(pxy->sendq[idx], sizeof(rtmq_header_t)+size);
    if (NULL == addr) {
        log_error(pxy->log, "Alloc from queue failed! size:%d/%d",
                size+sizeof(rtmq_header_t), queue_size(pxy->sendq[idx]));
        return RTMQ_ERR;
    }

    /* > 设置发送数据 */
    head = (rtmq_header_t *)addr;

    head->type = type;
    head->nid = conf->nid;
    head->length = size;
    head->flag = RTMQ_EXP_MESG;
    head->chksum = RTMQ_CHKSUM_VAL;

    memcpy(head+1, data, size);

    log_debug(pxy->log, "rq:%p Head type:%d nid:%d length:%d flag:%d chksum:%d!",
            pxy->sendq[idx]->ring, head->type, head->nid, head->length, head->flag, head->chksum);

    /* > 放入发送队列 */
    if (queue_push(pxy->sendq[idx], addr)) {
        log_error(pxy->log, "Push into shmq failed!");
        queue_dealloc(pxy->sendq[idx], addr);
        return RTMQ_ERR;
    }

    /* > 通知发送线程 */
    rtmq_proxy_cli_cmd_send_req(pxy, idx);

    return RTMQ_OK;
}
