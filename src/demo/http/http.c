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

    memset(&field, 0, sizeof(field));

    http_get_request(argv[1], get_req, sizeof(get_req));

    ret = uri_reslove(argv[1], &field);

    fprintf(stdout, "P:%s H:%s P:%d P:%s R:%d\n",
            field.protocol, field.host, field.port, field.path, ret);
    return 0;
}
