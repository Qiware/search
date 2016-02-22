/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: shm_queue_demo.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Wed 06 May 2015 03:21:22 PM CST #
 ******************************************************************************/

#include "shm_queue.h"
#include "sig_queue.h"
#include "thread_pool.h"

#define QUEUE_LEN       (1024*1024*32)
#define QUEUE_CHECK_SUM (0x12345678)

typedef struct
{
    int check;              /* 校验值 */
    int seq;                /* 序列号 */
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
    if (NULL == log) {
        fprintf(stderr, "Init log failed! level:%d", level);
        return NULL;
    }

    return log;
}

void *shm_queue_push_routine(void *str)
{
    int idx, max = QUEUE_LEN;
    void *addr;
    static int seq = 0;
    shm_queue_t *queue;
    queue_header_t *head;
    char path[FILE_PATH_MAX_LEN];

    /* > 创建共享内存队列 */
    snprintf(path, sizeof(path), "%s.key", (const char *)str);

    queue = shm_queue_attach(path);
    if (NULL == queue) {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return (void *)-1;
    }

    for (idx=0; idx<max; ++idx) {
        addr = shm_queue_malloc(queue, sizeof(queue_header_t));
        if (NULL == addr) {
            usleep(0);
            continue;
        }

        head = (queue_header_t *)addr;

        head->check = QUEUE_CHECK_SUM;
        head->seq = ++seq;

        shm_queue_push(queue, addr);
    }

    return (void *)0;
}

void *shm_queue_pop_routine(void *q)
{
    int idx = 0, max = QUEUE_LEN;
    void *addr;
    queue_header_t *head;
    shm_queue_t *queue = (shm_queue_t *)q;

    while (1) {
        addr = shm_queue_pop(queue);
        if (NULL == addr) {
            continue;
        }

        head = (queue_header_t *)addr;
        if (QUEUE_CHECK_SUM != head->check) {
            abort();
        }
        //fprintf(stdout, "seq:%d\n", head->seq);

        shm_queue_dealloc(queue, addr);
        if (++idx == max) {
            exit(0);
        }
    }

    return (void *)0;
}

int shm_queue_test(int argc, char *argv[], log_cycle_t *log)
{
    pthread_t tid;
    shm_queue_t *queue;
    char path[FILE_PATH_MAX_LEN];

    /* > 创建共享内存队列 */
    snprintf(path, sizeof(path), "%s.key", basename(argv[0]));

    queue = shm_queue_creat(path, QUEUE_LEN, sizeof(queue_header_t));
    if (NULL == queue) {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    shm_queue_print(queue);

    /* > 操作共享内存队列(申请 放入 弹出 回收等操作) */
    thread_creat(&tid, shm_queue_push_routine, (void *)basename(argv[0]));
    thread_creat(&tid, shm_queue_pop_routine, (void *)queue);

    return 0;
}

void *sig_queue_push_routine(void *_queue)
{
    int idx, max = QUEUE_LEN;
    void *addr;
    static int seq = 0;
    sig_queue_t *queue;
    queue_header_t *head;

    queue = (sig_queue_t *)_queue;

    for (idx=0; idx<max; ++idx) {
        addr = sig_queue_malloc(queue, sizeof(queue_header_t));
        if (NULL == addr) {
            continue;
        }

        head = (queue_header_t *)addr;

        head->check = QUEUE_CHECK_SUM;
        head->seq = ++seq;

        sig_queue_push(queue, addr);
    }

    return (void *)0;
}

void *sig_queue_pop_routine(void *q)
{
    int idx = 0, max = QUEUE_LEN;
    void *addr;
    queue_header_t *head;
    sig_queue_t *queue = (sig_queue_t *)q;

    while (1) {
        addr = sig_queue_pop(queue);
        if (NULL == addr) {
            continue;
        }

        head = (queue_header_t *)addr;
        if (QUEUE_CHECK_SUM != head->check) {
            abort();
        }

        //fprintf(stdout, "seq:%d\n", head->seq);

        sig_queue_dealloc(queue, addr);
        if (++idx == max) {
            exit(0);
        }
    }

    return (void *)0;
}

int sig_queue_test(int argc, char *argv[], log_cycle_t *log)
{
    pthread_t tid;
    sig_queue_t *queue;

    queue = sig_queue_creat(QUEUE_LEN, sizeof(queue_header_t));
    if (NULL == queue) {
        fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
        return -1;
    }

    sig_queue_print(queue);

    /* > 操作共享内存队列(申请 放入 弹出 回收等操作) */
    thread_creat(&tid, sig_queue_push_routine, (void *)queue);
    thread_creat(&tid, sig_queue_pop_routine, (void *)queue);

    return 0;
}

int main(int argc, char *argv[])
{
    log_cycle_t *log;

    /* > 初始化日志模块 */
    log = demo_init_log(basename(argv[0]));
    if (NULL == log) {
        return -1;
    }

#if 1
    shm_queue_test(argc, argv, log);
#else
    sig_queue_test(argc, argv, log);
#endif
    while (1) { pause(); }

    return 0;
}
