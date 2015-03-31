#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "http.h"
#include "xd_str.h"


int main(int argc, char *argv[])
{
    int ret;
    char get_req[4096];
    uri_field_t field;
    const char *site = "http://www.baidu.com/news";
    const char *href = "javascript(0):1";

    memset(&field, 0, sizeof(field));

    http_get_request(argv[1], get_req, sizeof(get_req));

    ret = uri_reslove(argv[1], &field);

    href_to_uri(href, site, &field);

    fprintf(stdout, "P:%d H:%s P:%d P:%s R:%d\n",
            field.protocol, field.host, field.port, field.path, ret);
    return 0;
}
