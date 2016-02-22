#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "gumbo_ex.h"
#include <gumbo.h>

int main(int argc, const char** argv)
{
    int ret;
    gumbo_cntx_t ctx;
    gumbo_html_t *html;
    const char *fname = argv[1], *title;

    if (2 != argc) {
        printf("Usage: get_title <html fname>.\n");
        exit(EXIT_FAILURE);
    }

    log2_init("trace", "./gettile.log");

    ret = gumbo_init(&ctx);
    if (0 != ret) {
        fprintf(stderr, "Init gumbo failed!");
        return -1;
    }

    html = gumbo_html_parse(&ctx, fname);
    if (NULL == html) {
        gumbo_destroy(&ctx);
        fprintf(stderr, "Parse html failed! [%s]", fname);
        return -1;
    }

#if 1
    title = gumbo_get_title(html);
    printf("%s\n", title);
    gumbo_get_href(html);
#endif

    gumbo_html_destroy(&ctx, html);
    gumbo_destroy(&ctx);
    return 0;
}
