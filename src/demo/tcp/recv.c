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

/****************************************************************************** 
 **函数名称: tcp_listen                                                                                        
 **功    能: 侦听指定端口                                                                                      
 **输入参数:                                                                                                   
 **     port: 端口号                                                                                           
 **输出参数: NONE                                                                                              
 **返    回: 套接字ID                                                                                          
 **实现描述:                                                                                                   
 **     1. 创建套接字                                                                                          
 **     2. 绑定指定端口                                                                                        
 **     3. 侦听指定端口                                                                                        
 **     4. 设置套接字属性(可重用、非阻塞)                                                                      
 **注意事项:                                                                                                   
 **作    者: # Qifeng.zou # 2014.03.24 #                                                                       
 ******************************************************************************/
int tcp_listen(int port)                                                                                       
{                                                                                                              
    int fd, opt = 1;                                                                                           
    struct sockaddr_in svraddr;                                                                                

    /* 1. 创建套接字 */                                                                                        
    fd = socket(AF_INET, SOCK_STREAM, 0);                                                                      
    if (fd < 0)                                                                                                
    {                                                                                                          
        return -1;
    }                                                                           
                                                                                
    opt = 1;                                                                    
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));                
                                                                                
    /* 2. 绑定指定端口 */                                                       
    bzero(&svraddr, sizeof(svraddr));                                           
                                                                                
    svraddr.sin_family = AF_INET;                                               
    svraddr.sin_addr.s_addr = htonl(INADDR_ANY);                                
    svraddr.sin_port = htons(port);                                             
                                                                                
    if (bind(fd, (struct sockaddr *)&svraddr, sizeof(svraddr)) < 0)             
    {                                                                           
        close(fd);                                                              
        return -1;                                                              
    }                                                                           
                                                                                
    /* 3. 侦听指定端口 */                                                       
    if (listen(fd, 20) < 0)                                                     
    {                                                                           
        close(fd);                                                              
        return -1;                                                              
    }                                                                           
                                                                                
    /* 4. 设置非阻塞属性 */                                                     
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
                                                                                
    return fd;                                                                  
}           



int main(int argc, char *argv[])
{
    int ret, n, max, lsnid, sckid = -1;
    fd_set rdset;
    struct timeval timeout;
    socklen_t len;
    struct sockaddr_in cliaddr;
    char buff[1500];


    lsnid = tcp_listen(atoi(argv[1]));
    if (lsnid < 0)
    {
        return -1;
    }

    while(1)
    {
        FD_ZERO(&rdset);

        FD_SET(lsnid, &rdset);

        if (sckid > 0)
        {
            FD_SET(sckid, &rdset);
        }

        max = lsnid > sckid? lsnid : sckid;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;

        ret = select(max+1, &rdset, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (EINTR == errno) { continue; }
            return -1;
        }
        else if (0 == ret)
        {
            continue;
        }
        

        if (FD_ISSET(lsnid, &rdset))
        {
            len = sizeof(cliaddr);

            sckid = accept(lsnid, (struct sockaddr *)&cliaddr, &len);
            if (sckid < 0)
            {
            }
        }

        if (sckid > 0 && FD_ISSET(sckid, &rdset))
        {
            while (1)
            {
                n = read(sckid, buff, sizeof(buff));
                if (n < 0)
                {
                    break;
                }
            }
        }
    }

    close(lsnid);
    close(sckid);
    return 0;
}
