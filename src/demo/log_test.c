#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

int main(void)
{
    int idx;
    log_cycle_t *log;

    log = log_init("error", "../log/log_test.log");
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
