#include "sck.h"

int main(int argc, char *argv[])
{
    int ret, n, max, lsnid, sckid = -1;
    fd_set rdset;
    struct timeval timeout;
    socklen_t len;
    struct sockaddr_in cliaddr;
    char buff[1500];


    lsnid = tcp_listen(atoi(argv[1]));
    if (lsnid < 0) {
        return -1;
    }

    while(1) {
        FD_ZERO(&rdset);
        FD_SET(lsnid, &rdset);

        if (sckid > 0) {
            FD_SET(sckid, &rdset);
        }

        max = lsnid > sckid? lsnid : sckid;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;

        ret = select(max+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0) {
            if (EINTR == errno) { continue; }
            return -1;
        }
        else if (0 == ret) {
            continue;
        }
        

        if (FD_ISSET(lsnid, &rdset)) {
            len = sizeof(cliaddr);

            sckid = accept(lsnid, (struct sockaddr *)&cliaddr, &len);
            if (sckid < 0) {
                frpintf(stderr, "errmsg:[%d] %s\n", errno, strerror(errno));
                return -1;
            }
        }

        if (sckid > 0 && FD_ISSET(sckid, &rdset)) {
            while (1) {
                n = read(sckid, buff, sizeof(buff));
                if (n < 0) {
                    break;
                }
            }
        }
    }

    close(lsnid);
    close(sckid);
    return 0;
}
