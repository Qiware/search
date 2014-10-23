#if !defined(__COMMON_H__)
#define __COMMON_H__

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#if !defined(bool)
    #define bool char
#endif
#if !defined(false)
    #define false (0)
#endif
#if !defined(true)
    #define true  (1)
#endif

/* 宏定义 */
#define FILE_NAME_MAX_LEN   (256)   /* 文件名最大长度 */
#define PATH_NAME_MAX_LEN   (256)   /* 路径最大长度 */
#define FILE_LINE_MAX_LEN   (256)   /* 文件行最大长度 */
#define FILE_PATH_MAX_LEN   (256)   /* 文件路径最大长度 */
#define IP_ADDR_MAX_LEN     (64)    /* IP地址最大长度 */
#define CMD_LINE_MAX_LEN    (1024)  /* 命令行最大长度 */
#define QUEUE_NAME_MAX_LEN  (64)    /* 队列名最大长度 */
#define TABLE_NAME_MAX_LEN  (64)    /* 表名最大长度 */
#define ERR_MSG_MAX_LEN     (1024)  /* 错误信息最大长度 */
#define URL_MIN_LEN         (3)     /* URL最小长度 */
#define URL_MAX_LEN         (4096)  /* URL最大长度 */
#define URI_MIN_LEN     URI_MIN_LEN /* URI最小长度 */
#define URI_MAX_LEN     URL_MAX_LEN /* URI最大长度 */
#define URI_PROTOCOL_MAX_LEN    (32)/* 协议类型 */

#define MD5_SUM_CHK_LEN     (32)    /* MD5校验值长度 */

#define INVALID_FD          (-1)    /* 非法文件描述符 */
#define INVALID_PID         (-1)    /* 非法进程ID */

#define IPV4                (0)     /* IP地址类型-IPV4 */
#define IPV6                (1)     /* IP地址类型-IPV6 */

/* 内存单位 */
#define KB                  (1024)      /* KB */
#define MB                  (1024 * KB) /* MB */
#define GB                  (1024 * MB) /* GB */

/* 字符定义 */
#define is_uline_char(ch)   ('_' == ch)     /* 下划线 */
#define is_tab_char(ch)     ('\t' == ch)    /* 制表符 */
#define is_end_char(ch)     ('\0' == ch)    /* 结束符 */
#define is_equal_char(ch)   ('=' == ch)     /* 等号符 */
#define is_dquot_char(ch)   ('"' == ch)     /* 双引号 */
#define is_squot_char(ch)   ('\'' == ch)    /* 单引号 */
#define is_quot_char(ch)    (('"' == ch) || ('\'' == ch))   /* 引号 */
#define is_lbrack_char(ch)  ('<' == ch)     /* 左尖括号 */
#define is_rbrack_char(ch)  ('>' == ch)     /* 右尖括号 */
#define is_rslash_char(ch)  ('/' == ch)     /* 右斜线 */
#define is_doubt_char(ch)   ('?' == ch)     /* 疑问号 */
#define is_and_char(ch)     ('&' == ch)     /* 与号 */
#define is_sub_char(ch)     ('-' == ch)     /* 减号 */
#define is_colon_char(ch)   (':' == ch)     /* 冒号 */
#define is_nline_char(ch)   (('\n'== ch) || ('\r' == ch))  /* 换行符 */

/* 内存对齐 */
#define mem_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define mem_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define PTR_ALIGNMENT   sizeof(unsigned long)

/* 协议类型 */
typedef enum
{
    PROTOCOL_UNKNOWN                        /* 未知协议 */
    , PROTOCOL_HTTP                         /* HTTP协议 */
    , PROTOCOL_HTTPS                        /* HTTPS协议 */
    , PROTOCOL_FTP                          /* FTP协议 */
} protocol_type_e;

#endif /*__COMMON_H__*/
