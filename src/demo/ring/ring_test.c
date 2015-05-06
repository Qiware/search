/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: ring_test.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # Tue 05 May 2015 10:13:06 PM CST #
 ******************************************************************************/

#include "queue.h"

#define QUEUE_LEN       (1024)
#define QUEUE_SIZE      (1024)

int main(int argc, char *argv[])
{
    int i, level;
    void *addr;
    queue_t *queue;
    log_cycle_t *log;
    char path[FILE_PATH_MAX_LEN];

    level = log_get_level("error");

    snprintf(path, sizeof(path), "../log/%s.log", basename(argv[0]));

    log = log_init(level, path);
    if (NULL == log)
    {
        fprintf(stderr, "Init log failed!");
        return -1;
    }

    syslog_init(2, "../log/syslog.log");

    queue = queue_creat(QUEUE_LEN, QUEUE_SIZE);
    if (NULL == queue)
    {
        return -1;
    }

    queue_print(queue);

    for (i=0; i<QUEUE_LEN; ++i)
    {
        addr = queue_malloc(queue);

        log_error(log, "alloc1: %p", addr);

        queue_dealloc(queue, addr);
    }

    for (i=0; i<QUEUE_LEN; ++i)
    {
        addr = queue_malloc(queue);

        log_error(log, "alloc2: %p", addr);

        queue_dealloc(queue, addr);
    }

    queue_destroy(queue);
    return 0;
}
