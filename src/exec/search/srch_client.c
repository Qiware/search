#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "xds_socket.h"

#define SRCH_SVR_IP_ADDR    "127.0.0.1"
#define SRCH_SVR_PORT       (8888)

#define SRCH_CLIENT_NUM     (1000)

int main(int argc, char *argv[])
{
    int fd[SRCH_CLIENT_NUM], idx;
    const char *ip = SRCH_SVR_IP_ADDR;
    int port = SRCH_SVR_PORT;

    if (3 == argc)
    {
        ip = argv[1];
        port = atoi(argv[2]);
    }

    for (idx=0; idx<SRCH_CLIENT_NUM; ++idx)
    {
        fd[idx] = tcp_connect(AF_INET, ip, port);
        if (fd[idx] < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
    }

    sleep(8);

    for (idx=0; idx<SRCH_CLIENT_NUM; ++idx)
    {
        close(fd[idx]);
    }

    while (1) { pause(); }

    return 0;
}
