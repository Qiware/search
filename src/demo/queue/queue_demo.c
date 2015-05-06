/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_queue_demo.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 03:21:22 PM CST #
 ******************************************************************************/

#include "shm_queue.h"

#define QUEUE_LEN       (16)
#define QUEUE_SIZE      (1024)

/* 初始化日志模块 */
log_cycle_t *demo_init_log(const char *_path)
{
    int level;
    log_cycle_t *log;
    char path[FILE_PATH_MAX_LEN];

    level = log_get_level("debug");

    snprintf(path, sizeof(path), "%s.log", _path);

    log = log_init(level, path);
    if (NULL == log)
    {
        fprintf(stderr, "Init log failed! level:%d", level);
        return NULL;
    }

    snprintf(path, sizeof(path), "%s.plog", _path);

    plog_init(level, path);

    return log;
}

int main(int argc, char *argv[])
{
    int i;
    key_t key;
    log_cycle_t *log;
    shm_queue_t *queue;
    void *addr[QUEUE_LEN], *addr2[QUEUE_LEN];

    /* > 初始化日志模块 */
    log = demo_init_log(basename(argv[0]));
    if (NULL == log)
    {
        return -1;
    }

    /* > 创建共享内存队列 */
    key = ftok(basename(argv[0]), 0);
    if (-1 == key)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    queue = shm_queue_creat(key, QUEUE_LEN, QUEUE_SIZE);
    if (NULL == queue)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    shm_queue_print(queue);

    /* > 操作共享内存队列(申请 放入 弹出 回收等操作) */
    for (i=0; i<QUEUE_LEN; ++i)
    {
        addr[i] = shm_queue_malloc(queue);

        shm_queue_push(queue, addr[i]);
    }

    for (i=0; i<QUEUE_LEN; ++i)
    {
        addr2[i] = shm_queue_pop(queue);
        if (addr[i] != addr2[i])
        {
            assert(0);
        }

        shm_queue_dealloc(queue, addr2[i]);
    }

    return 0;
}
