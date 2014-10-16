#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "http.h"


int main(int argc, char *argv[])
{
    char get_req[4096];

    http_get_request(argv[1], get_req, sizeof(get_req));

    return 0;
}
