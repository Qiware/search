#include "sck.h"

int main(int argc, char *argv[])
{
    int n, max, ret, sckid;
    fd_set wrset;
    char buff[1500];
    struct timeval timeout;

    sckid = tcp_connect(AF_INET, "127.0.0.1", atoi(argv[1]));
    if (sckid < 0)
    {
        return -1;
    }

    while(1)
    {
        FD_ZERO(&wrset);

        FD_SET(sckid, &wrset);

        max = sckid;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;

        ret = select(max+1, NULL, &wrset, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            return -1;
        }
        else if (0 == ret)
        {
            continue;
        }

        if (FD_ISSET(sckid, &wrset))
        {
            while (1)
            {
                n = write(sckid, buff, sizeof(buff));
                if (n < 0)
                {
                    break;
                }
            }
        }
    }
}
