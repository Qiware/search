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

#include "syscall.h"
#include "srch_mesg.h"
#include "xds_socket.h"

#define SRCH_SVR_IP_ADDR    "127.0.0.1"
#define SRCH_SVR_PORT       (8888)

#define SRCH_CLIENT_NUM     (2000)

int main(int argc, char *argv[])
{
    int fd[SRCH_CLIENT_NUM], idx, num = SRCH_CLIENT_NUM;
    const char *ip = SRCH_SVR_IP_ADDR;
    int n, port = SRCH_SVR_PORT;
    srch_mesg_header_t header;
    srch_mesg_body_t body;

    if (3 == argc)
    {
        ip = argv[1];
        port = atoi(argv[2]);
    }
    if (4 == argc)
    {
        ip = argv[1];
        port = atoi(argv[2]);
        num = atoi(argv[3]);
    }

    limit_file_num(4096); /* 设置进程打开文件的最大数目 */

    for (idx=0; idx<num; ++idx)
    {
        fd[idx] = tcp_connect(AF_INET, ip, port);
        if (fd[idx] < 0)
        {
            fprintf(stderr, "errmsg:[%d] %s!", errno, strerror(errno));
            return -1;
        }
    }

    for (idx=0; idx<SRCH_CLIENT_NUM; ++idx)
    {
        header.type = idx%0xFF;
        header.flag = SRCH_MSG_FLAG_USR;
        header.mark = htonl(SRCH_MSG_MARK_KEY);
        header.length = htons(sizeof(body));

        snprintf(body.words, sizeof(body.words), "爱我中华");

        n = Writen(fd[idx], (void *)&header, sizeof(header));

        n = Writen(fd[idx], (void *)&body, sizeof(body));

        fprintf(stdout, "idx:%d n:%d!\n", idx, n);
    }

    while (1) { pause(); }

    return 0;
}
