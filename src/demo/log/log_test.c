#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>

#include "log.h"
#include "syscall.h"

int main(int argc, char *argv[])
{
    int idx, level;
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

    log2_init("error", "../log/syslog.log");

    for (idx=0; idx<1000000; ++idx)
    {
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

    log_destroy(&log);

    return 0;
}
