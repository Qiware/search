#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "list.h"
#include "gumbo_ex.h"
#include <gumbo.h>

int main(int argc, const char** argv)
{
    int ret;
    gumbo_cntx_t ctx;
    gumbo_html_t *html;
    gumbo_result_t *r;
    const char *fname = argv[1];

    if (2 != argc)
    {
        printf("Usage: get_title <html fname>.\n");
        exit(EXIT_FAILURE);
    }

    log2_init("trace", "./gumbo.log");

    ret = gumbo_init(&ctx);
    if (0 != ret)
    {
        fprintf(stderr, "Init gumbo failed!");
        return -1;
    }

    html = gumbo_html_parse(&ctx, fname);
    if (NULL == html)
    {
        gumbo_destroy(&ctx);
        fprintf(stderr, "Parse html failed! [%s]", fname);
        return -1;
    }

    r = gumbo_parse_href(&ctx, html);
    if (NULL == r)
    {
        gumbo_html_destroy(&ctx, html);
        gumbo_destroy(&ctx);
        fprintf(stderr, "Search href failed!");
        return -1;
    }

    gumbo_print_result(r);

    gumbo_result_destroy(&ctx, r);
    gumbo_html_destroy(&ctx, html);
    gumbo_destroy(&ctx);
    return 0;
}
