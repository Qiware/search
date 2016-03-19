#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>

#include "log.h"
#include "syscall.h"

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

    snprintf(path, sizeof(path), "%s.plog", _path);

    plog_init(level, path);

    return log;
}

int main(int argc, char *argv[])
{
    int idx;
    log_cycle_t *log;

    log = demo_init_log(basename(argv[0]));

    for (idx=0; idx<1000000; ++idx) {
    #if 0
        log2_error("This is just a test! [%d]", idx);
    #else
        log_fatal(log, "This is just a test! [%d]", idx);
        log_error(log, "This is just a test! [%d]", idx);
        log_warn(log, "This is just a test! [%d]", idx);
        log_info(log, "This is just a test! [%d]", idx);
        log_debug(log, "This is just a test! [%d]", idx);
        log_info(log, "This is just a test! [%d]", idx);
    #endif
    }

    return 0;
}
