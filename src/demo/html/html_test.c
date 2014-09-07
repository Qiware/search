#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>

#include "html_tree.h"

int main(int argc, char *argv[])
{
    int level;
    log_cycle_t *log;
    html_tree_t *html;
    char path[FILE_PATH_MAX_LEN];

    if (2 != argc)
    {
        fprintf(stderr, "Param isn't right!");
        return -1;
    }

    level = log_get_level("trace");

    snprintf(path, sizeof(path), "../log/%s.log", basename(argv[0]));

    log = log_init(level, path);
    if (NULL == log)
    {
        fprintf(stderr, "Init log failed!");
        return -1;
    }

    html = html_creat(argv[1], log);
    if (NULL == html)
    {
        fprintf(stderr, "Create html tree failed!");
        log_destroy(&log);
        return -1;
    }

    html_destroy(html);

    //log_destroy(&log);
    return 0;
}
