#include "shm_opt.h"
#include "syscall.h"
#include "rttp_cmd.h"
#include "rtsd_cli.h"
#include "rtsd_ssvr.h"
#include "rttp_comm.h"

static int _rtsd_cli_init(rtsd_cli_t *cli, int idx);
static int rtsd_cli_shmat(rtsd_cli_t *cli);
static int rtsd_cli_cmd_usck(rtsd_cli_t *cli, int idx);

#define rtsd_cli_unix_path(cli, path, idx) \
    snprintf(path, sizeof(path), "../temp/rttp/send/%s/%s_cli_%d.usck", cli->conf.name, cli->conf.name, idx)

/******************************************************************************
 **函数名称: rtsd_cli_init
 **功    能: 发送端初始化(对外接口)
 **输入参数:
 **     conf: 配置信息
 **     idx: CLI编号
 **     log: 日志对象
 **输出参数: NONE
 **返    回: 发送对象
 **实现描述:
 **     1. 创建CLI对象
 **     2. 加载配置信息
 **     3. 初始化处理
 **注意事项: 某发送服务的不同cli对象的编号必须不同，否则将会出现绑定失败的问题!
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
rtsd_cli_t *rtsd_cli_init(const rtsd_conf_t *conf, int idx, log_cycle_t *log)
{
    rtsd_cli_t *cli;
    mem_pool_t *pool;

    /* 1. 创建内存池 */
    pool = mem_pool_creat(1 * KB);
    if (NULL == pool)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 创建CLI对象 */
    cli = (rtsd_cli_t *)mem_pool_alloc(pool, sizeof(rtsd_cli_t));
    if (NULL == cli)
    {
        log_error(log, "Alloc memory from pool failed!");
        mem_pool_destroy(pool);
        return NULL;
    }

    cli->log = log;
    cli->pool = pool;

    /* 2. 加载配置信息 */
    memcpy(&cli->conf, conf, sizeof(rtsd_conf_t));

    /* 3. 根据配置进行初始化 */
    if (_rtsd_cli_init(cli, idx))
    {
        log_error(log, "Initialize client of rttp failed!");
        mem_pool_destroy(pool);
        return NULL;
    }

    return cli;
}

/******************************************************************************
 **函数名称: _rtsd_cli_init
 **功    能: 发送端初始化
 **输入参数:
 **     cli: CLI对象
 **     idx: CLI编号
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int _rtsd_cli_init(rtsd_cli_t *cli, int idx)
{
    /* 1. 连接共享内存
     * 2. 创建通信套接字 */
    if (rtsd_cli_shmat(cli)
        || rtsd_cli_cmd_usck(cli, idx))
    {
        log_error(cli->log, "Initialize client of rttp failed!");
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtsd_cli_shmat
 **功    能: Attach发送队列
 **输入参数:
 **     cli: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtsd_cli_shmat(rtsd_cli_t *cli)
{
    int idx;
    shmq_conf_t *qcf;
    char path[FILE_NAME_MAX_LEN];
    rtsd_conf_t *conf = &cli->conf;

    /* 1. 新建队列对象 */
    cli->sendq = (shm_queue_t **)mem_pool_alloc(cli->pool, conf->send_thd_num * sizeof(shm_queue_t *));
    if (NULL == cli->sendq)
    {
        log_error(cli->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return RTTP_ERR;
    }

    /* 2. 连接共享队列 */
    qcf = &conf->sendq;
    for (idx=0; idx<conf->send_thd_num; ++idx)
    {
        snprintf(path, sizeof(path), "%s-%d", qcf->path, idx);

        cli->sendq[idx] = shm_queue_attach(path);
        if (NULL == cli->sendq[idx])
        {
            log_error(cli->log, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), qcf->path);
            return RTTP_ERR;
        }
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtsd_cli_cmd_usck
 **功    能: 创建命令套接字
 **输入参数:
 **     cli: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtsd_cli_cmd_usck(rtsd_cli_t *cli, int idx)
{
    char path[FILE_NAME_MAX_LEN];

    rtsd_cli_unix_path(cli, path, idx);

    cli->cmd_sck_id = unix_udp_creat(path);
    if (cli->cmd_sck_id < 0)
    {
        log_error(cli->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return RTTP_ERR;
    }

    return RTTP_OK;
}

/******************************************************************************
 **函数名称: rtsd_cli_cmd_send_req
 **功    能: 通知Send服务线程
 **输入参数:
 **     cli: 上下文信息
 **     idx: 发送服务ID
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int rtsd_cli_cmd_send_req(rtsd_cli_t *cli, int idx)
{
    rttp_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    rtsd_conf_t *conf = &cli->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = RTTP_CMD_SEND_ALL;

    rtsd_ssvr_usck_path(conf, path, idx);

    return unix_udp_send(cli->cmd_sck_id, path, &cmd, sizeof(cmd));
}

/******************************************************************************
 **函数名称: rtsd_cli_send
 **功    能: 发送指定数据(对外接口)
 **输入参数:
 **     cli: 上下文信息
 **     type: 数据类型
 **     devid: 源设备ID
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
int rtsd_cli_send(rtsd_cli_t *cli, int type, const void *data, size_t size)
{
    int idx;
    void *addr;
    rttp_header_t *head;
    static uint8_t num = 0;
    rtsd_conf_t *conf = &cli->conf;

    /* > 选择发送队列 */
    idx = (num++) % conf->send_thd_num;

    addr = shm_queue_malloc(cli->sendq[idx]);
    if (NULL == addr)
    {
        log_error(cli->log, "Alloc from shmq failed!");
        return RTTP_ERR;
    }

    /* > 设置发送数据 */
    head = (rttp_header_t *)addr;

    head->type = type;
    head->devid = conf->auth.devid;
    head->length = size;
    head->flag = RTTP_EXP_MESG;
    head->checksum = RTTP_CHECK_SUM;

    memcpy(head+1, data, size);

    plog_debug("rq:%p Head type:%d devid:%d length:%d flag:%d checksum:%d!",
            cli->sendq[idx]->ring, head->type, head->devid, head->length, head->flag, head->checksum);
    /* > 放入发送队列 */
    if (shm_queue_push(cli->sendq[idx], addr))
    {
        log_error(cli->log, "Push into shmq failed!");
        shm_queue_dealloc(cli->sendq[idx], addr);
        return RTTP_ERR;
    }

    /* > 通知发送线程 */
    rtsd_cli_cmd_send_req(cli, idx);

    return RTTP_OK;
}
