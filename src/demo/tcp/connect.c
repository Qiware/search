#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sck.h"
#include "comm.h"

#define PORT (80)

int main(int argc, char *argv[])
{
#if 0
    int port, fd;
    char ipaddr[IP_ADDR_MAX_LEN];

    if (3 != argc) {
        return -1;
    }

    snprintf(ipaddr, sizeof(ipaddr), "%s", argv[1]);
    port = atoi(argv[2]);

    fd = tcp_connect(AF_INET, ipaddr, port);

    fprintf(stderr, "errmsg:[%d] %s! fd:%d", errno, strerror(errno), fd);
#else
    int i = 0;
    struct in6_addr ip;
    char ip_str[20];

    inet_pton(AF_INET6, "2a01:198:603:0:396e:4789:8e99:890f", &ip);
    for(i = 0; i < 16; i ++) {
        printf("0x%x ", ip.s6_addr[i]);
    }

    printf("\n");
    inet_pton(AF_INET6, "2a01:198:603:0::", &ip);
    for(i = 0; i < 16; i ++) {
        printf("0x%x ", ip.s6_addr[i]);
    }

    printf("\n");
    inet_pton(AF_INET6, "::ffff:127.0.0.1", &ip);
    for(i = 0; i < 16; i ++) {
        printf("0x%x ", ip.s6_addr[i]);
    }
    inet_ntop(AF_INET6, &ip, ip_str, sizeof(ip_str));
    printf("out:%s\n", ip_str);
#endif

    return 0;
}
