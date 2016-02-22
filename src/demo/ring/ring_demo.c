/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: ring_test.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Tue 05 May 2015 10:13:06 PM CST #
 ******************************************************************************/

#include "queue.h"

#define QUEUE_LEN       (1024)
#define QUEUE_SIZE      (8192)

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

int main(int argc, char *argv[])
{
    int i, n;
    void *addr;
    queue_t *queue;
    log_cycle_t *log;

    log = demo_init_log(basename(argv[0]));
    if (NULL == log) {
        fprintf(stderr, "Init log failed!");
        return -1;
    }

    queue = queue_creat(QUEUE_LEN, QUEUE_SIZE);
    if (NULL == queue) {
        fprintf(stderr, "Create queue failed!");
        return -1;
    }

    queue_print(queue);

    for (n=0; n<5; ++n) {
        for (i=0; i<QUEUE_LEN; ++i) {
            addr = queue_malloc(queue, QUEUE_SIZE);
            if (NULL == addr) {
                break;
            }

            queue_push(queue, addr);

            log_error(log, "alloc1: %p", addr);

            addr = queue_pop(queue);

            queue_dealloc(queue, addr);

            log_error(log, "alloc1: %p", addr);
        }
    }

    queue_destroy(queue);
    return 0;
}
