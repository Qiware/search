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

int tcp_connect(int family, const char *ipaddr, int port)                       
{                                                                                                              
    int fd, opt = 1;                                                                                           
    struct sockaddr_in svraddr;                                                                                
                                                                                                               
    /* 1. 创建套接字 */                                                                                        
    fd = socket(family, SOCK_STREAM, 0);                                                                       
    if (fd < 0)                                                                                                
    {                                                                                                          
        return -1;                                                                                             
    }                                                                                                          
                                                                                                               
    opt = 1;                                                                                                   
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));                
                                                                                                               
    /* 2. 连接远程服务器 */                                                                                    
    bzero(&svraddr, sizeof(svraddr));                                                                          
                                                                                                               
    svraddr.sin_family = AF_INET;                                                                              
    inet_pton(AF_INET, ipaddr, &svraddr.sin_addr);                              
    svraddr.sin_port = htons(port);                                                                            
                                                                                                               
    if (0 != connect(fd, (struct sockaddr *)&svraddr, sizeof(svraddr)))         
    {                                                                                                          
        close(fd);                                                                                             
        return -1;                                                                                             
    }                                                                                                          
                                                                                                               
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
                                                                                                               
    return fd;                                                                                                 
} 

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
            if (EINTR == errno)
            {
                continue;
            }
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
