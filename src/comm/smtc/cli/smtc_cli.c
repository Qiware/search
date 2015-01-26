#include "shm_opt.h"
#include "syscall.h"
#include "smtc_cmd.h"
#include "smtc_cli.h"
#include "smtc_ssvr.h"
#include "smtc_priv.h"

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

static int _smtc_cli_init(smtc_cli_t *cli, int idx);
static int smtc_cli_shmat(smtc_cli_t *cli);
static int smtc_cli_cmd_usck(smtc_cli_t *cli, int idx);

#define smtc_cli_unix_path(cli, path, idx) \
    snprintf(path, sizeof(path), "./temp/smtc/snd/%s/usck/%s_cli_%d.usck", \
        cli->conf.name, cli->conf.name, idx)

/******************************************************************************
 **函数名称: smtc_cli_init
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
 **注意事项: 
 **     某发送服务的不同cli对象的编号必须不同，否则将会出现绑定失败的问题!
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
smtc_cli_t *smtc_cli_init(const smtc_ssvr_conf_t *conf, int idx, log_cycle_t *log)
{
    smtc_cli_t *cli;
    mem_pool_t *pool;

    /* 1. 创建内存池 */
    pool = mem_pool_creat(1 * KB);
    if (NULL == pool)
    {
        log_error(log, "errmsg:[%d] %s!", errno, strerror(errno));
        return NULL;
    }

    /* 2. 创建CLI对象 */
    cli = (smtc_cli_t *)mem_pool_alloc(pool, sizeof(smtc_cli_t));
    if (NULL == cli)
    {
        log_error(log, "Alloc memory from pool failed!");
        mem_pool_destroy(pool);
        return NULL;
    }

    cli->log = log;
    cli->pool = pool;
    
    /* 2. 加载配置信息 */
    memcpy(&cli->conf, conf, sizeof(smtc_ssvr_conf_t));

    /* 3. 根据配置进行初始化 */
    if (_smtc_cli_init(cli, idx))
    {
        log_error(log, "Initialize client of smtc failed!");
        mem_pool_destroy(pool);
        return NULL;
    }

    return cli;
}

/******************************************************************************
 **函数名称: _smtc_cli_init
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
static int _smtc_cli_init(smtc_cli_t *cli, int idx)
{
    /* 1. 连接共享内存
     * 2. 创建通信套接字 */
    if (smtc_cli_shmat(cli)
        || smtc_cli_cmd_usck(cli, idx))
    {
        log_error(cli->log, "Initialize client of smtc failed!");
        return SMTC_ERR;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_cli_shmat
 **功    能: Attach发送队列
 **输入参数: 
 **     cli: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_cli_shmat(smtc_cli_t *cli)
{
    int idx;
    key_t key;
    smtc_queue_conf_t *qcf;
    smtc_ssvr_conf_t *conf = &cli->conf;

    /* 1. 新建队列对象 */
    cli->sq = (shm_queue_t **)mem_pool_alloc(cli->pool, conf->snd_thd_num * sizeof(shm_queue_t *));
    if (NULL == cli->sq)
    {
        log_error(cli->log, "errmsg:[%d] %s!", errno, strerror(errno));
        return SMTC_ERR;
    }

    /* 2. 连接共享队列 */
    qcf = &conf->qcf;
    for (idx=0; idx<conf->snd_thd_num; ++idx)
    {
        key = shm_ftok(qcf->name, idx);

        cli->sq[idx] = shm_queue_attach(key);
        if (NULL == cli->sq[idx])
        {
            log_error(cli->log, "errmsg:[%d] %s! path:[%s]", errno, strerror(errno), qcf->name);
            return SMTC_ERR;
        }
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_cli_cmd_usck
 **功    能: 创建命令套接字
 **输入参数: 
 **     cli: 上下文信息
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
static int smtc_cli_cmd_usck(smtc_cli_t *cli, int idx)
{
    char path[FILE_NAME_MAX_LEN];

    smtc_cli_unix_path(cli, path, idx);

    cli->cmdfd = unix_udp_creat(path);
    if (cli->cmdfd < 0)
    {
        log_error(cli->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return SMTC_ERR;                    
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_cli_cmd_send_req
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
static int smtc_cli_cmd_send_req(smtc_cli_t *cli, int idx)
{
    smtc_cmd_t cmd;
    char path[FILE_NAME_MAX_LEN];
    smtc_ssvr_conf_t *conf = &cli->conf;

    memset(&cmd, 0, sizeof(cmd));

    cmd.type = SMTC_CMD_SEND_ALL;
    smtc_ssvr_usck_path(conf, path, idx);

    if (unix_udp_send(cli->cmdfd, path, &cmd, sizeof(cmd)) < 0)
    {
        log_debug(cli->log, "errmsg:[%d] %s! path:%s", errno, strerror(errno), path);
        return SMTC_ERR;
    }

    return SMTC_OK;
}

/******************************************************************************
 **函数名称: smtc_cli_send
 **功    能: 发送指定数据(对外接口)
 **输入参数: 
 **     cli: 上下文信息
 **     type: 数据类型
 **     data: 数据地址
 **     size: 数据长度
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **     将数据按照约定格式放入队列中
 **注意事项: 
 **     1. 只能用于发送自定义数据类型, 而不能用于系统数据类型
 **     2. 不用关注变量num在多线程中的值, 因其不影响安全性
 **作    者: # Qifeng.zou # 2015.01.14 #
 ******************************************************************************/
int smtc_cli_send(smtc_cli_t *cli, int type, const void *data, size_t size)
{
    void *addr;
    uint32_t idx;
    static uint32_t num = 0;
    smtc_header_t *header;
    smtc_ssvr_conf_t *conf = &cli->conf;

    idx = (num++) % conf->snd_thd_num;

    /* 1. 校验类型和长度 */
    if ((type >= SMTC_TYPE_MAX)
        || (size + sizeof(smtc_header_t) > cli->sq[idx]->info->size))
    {
        log_error(cli->log, "Type of length is invalid! type:%d size:%u", type, size);
        return SMTC_ERR;
    }

    /* 2. 申请存储空间 */
    addr = shm_queue_malloc(cli->sq[idx]);
    if (NULL == addr)
    {
        if (0 == num%2)
        {
            smtc_cli_cmd_send_req(cli, idx);
        }
        log_error(cli->log, "Queue space isn't enough!");
        return SMTC_ERR;
    }

    /* 3. 放入队列空间 */
    header = (smtc_header_t *)addr;
    header->type = type;
    header->length = size;
    header->flag = SMTC_EXP_MESG; /* 自定义类型 */
    header->checksum = SMTC_CHECK_SUM;

    memcpy(addr + sizeof(smtc_header_t), data, size);

    /* 4. 压入发送队列 */
    if (shm_queue_push(cli->sq[idx], addr))
    {
        log_error(cli->log, "Push data into queue failed!");
        shm_queue_dealloc(cli->sq[idx], addr);
        return SMTC_ERR;
    }

    /* 5. 通知发送服务 */
    if (0 == num % 50)
    {
        smtc_cli_cmd_send_req(cli, idx);
    }

    log_debug(cli->log, "[%d] Push Success! type:%d size:%u", num, type, size);

    return SMTC_OK;
}
