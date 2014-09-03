#if !defined(__COMMON_H__)
#define __COMMON_H__

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
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

typedef int bool;
#define false (0)
#define true  (1)

#define FILE_NAME_MAX_LEN   (256)   /* 文件名最大长度 */
#define FILE_LINE_MAX_LEN   (256)   /* 文件行最大长度 */
#define FILE_PATH_MAX_LEN   (256)   /* 文件路径最大长度 */
#define IP_ADDR_MAX_LEN     (16)    /* IP地址最大长度 */
#define CMD_LINE_MAX_LEN    (1024)  /* 命令行最大长度 */
#define ERR_MSG_MAX_LEN     (1024)  /* 错误信息最大长度 */

#define MD5_SUM_CHK_LEN     (32)    /* MD5校验值长度 */
#define INVALID_FD          (-1)    /* 非法文件描述符 */
#define INVALID_PID         (-1)    /* 非法进程ID */

#endif /*__COMMON_H__*/
