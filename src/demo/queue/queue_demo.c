/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: shm_queue_demo.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 03:21:22 PM CST #
 ******************************************************************************/

#include "shm_queue.h"
#include "thread_pool.h"

#define QUEUE_LEN       (2048)
#define QUEUE_SIZE      (4096)
#define QUEUE_CHECK_SUM (0x12345678)

typedef struct
{
    int check;         /* 校验值 */
} queue_header_t;


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

    return log;
}

void *queue_push_routine(void *str)
{
    void *addr;
    shm_queue_t *queue;
    queue_header_t *head;
    char path[FILE_PATH_MAX_LEN];

    /* > 创建共享内存队列 */
    snprintf(path, sizeof(path), "%s.key", (const char *)str);

    queue = shm_queue_attach(path);
    if (NULL == queue)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return (void *)-1;
    }


    while (1)
    {
        addr = shm_queue_malloc(queue);
        if (NULL == addr)
        {
            usleep(5000);
            continue;
        }

        head = (queue_header_t *)addr;

        head->check = QUEUE_CHECK_SUM;

        shm_queue_push(queue, addr);
    }

    return (void *)0;
}

void *queue_pop_routine(void *q)
{
    void *addr;
    queue_header_t *head;
    shm_queue_t *queue = (shm_queue_t *)q;

    while (1)
    {
        addr = shm_queue_pop(queue);
        if (NULL == addr)
        {
            usleep(5000);
            continue;
        }

        head = (queue_header_t *)addr;
        if (QUEUE_CHECK_SUM != head->check)
        {
            abort();
        }

        shm_queue_dealloc(queue, addr);
    }

    return (void *)0;
}

int main(int argc, char *argv[])
{
    pthread_t tid;
    log_cycle_t *log;
    shm_queue_t *queue;
    char path[FILE_PATH_MAX_LEN];

    /* > 初始化日志模块 */
    log = demo_init_log(basename(argv[0]));
    if (NULL == log)
    {
        return -1;
    }

    /* > 创建共享内存队列 */
    snprintf(path, sizeof(path), "%s.key", basename(argv[0]));

    queue = shm_queue_creat(path, QUEUE_LEN, QUEUE_SIZE);
    if (NULL == queue)
    {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    shm_queue_print(queue);

    /* > 操作共享内存队列(申请 放入 弹出 回收等操作) */
    thread_creat(&tid, queue_push_routine, (void *)basename(argv[0]));
    thread_creat(&tid, queue_pop_routine, (void *)queue);

    while (1) { pause(); }

    return 0;
}
