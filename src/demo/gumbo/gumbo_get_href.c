#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "list.h"
#include "gumbo_ex.h"
#include <gumbo.h>

int main(int argc, const char** argv)
{
    log_cycle_t *log;
    gumbo_html_t *html;
    gumbo_result_t *r;
    const char *fname = argv[1];

    if (2 != argc) {
        printf("Usage: get_title <html fname>.\n");
        exit(EXIT_FAILURE);
    }

    log = log_init(LOG_LEVEL_DEBUG, "./gumbo.log");
    if (NULL == log) {
        fprintf(stderr, "Initialize log failed!\n");
    }

    html = gumbo_html_parse(fname, log);
    if (NULL == html) {
        fprintf(stderr, "Parse html failed! [%s]\n", fname);
        return -1;
    }

    r = gumbo_parse_href(html, log);
    if (NULL == r) {
        gumbo_html_destroy(html);
        fprintf(stderr, "Search href failed!\n");
        return -1;
    }

    gumbo_print_result(r);

    gumbo_result_destroy(r);
    gumbo_html_destroy(html);
    return 0;
}
