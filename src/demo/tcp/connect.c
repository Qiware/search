#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "xdo_socket.h"

#define PORT (80)

int main(int argc, char *argv[])
{
    int port, fd;
    char ipaddr[IP_ADDR_MAX_LEN];

    if (3 != argc)
    {
        return -1;
    }

    snprintf(ipaddr, sizeof(ipaddr), "%s", argv[1]);
    port = atoi(argv[2]);

    fd = tcp_connect(ipaddr, port);

    fprintf(stderr, "errmsg:[%d] %s! fd:%d", errno, strerror(errno), fd);

    return 0;
}
