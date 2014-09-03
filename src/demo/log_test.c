#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

int main(void)
{
    int level, idx;
    log_cycle_t *log;

    level = LOG_LEVEL_TRACE
                | LOG_LEVEL_DEBUG
                | LOG_LEVEL_INFO
                | LOG_LEVEL_WARN
                | LOG_LEVEL_ERROR
                | LOG_LEVEL_FATAL;

    log = log_init(level, "../log/log_test.log");
    if (NULL == log)
    {
        fprintf(stderr, "Init log failed!");
        return -1;
    }

    for (idx=0; idx<1000000; ++idx)
    {
        log_fatal(log, "This is just a test! [%d]", idx);
        log_error(log, "This is just a test! [%d]", idx);
        log_warn(log, "This is just a test! [%d]", idx);
        log_info(log, "This is just a test! [%d]", idx);
        log_debug(log, "This is just a test! [%d]", idx);
        log_info(log, "This is just a test! [%d]", idx);
    }

    log_destroy(&log);

    return 0;
}
