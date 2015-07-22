#if !defined(__COMM_H__)
#define __COMM_H__

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/shm.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/resource.h>

/* 宏定义 */
#define FILE_NAME_MAX_LEN   (256)           /* 文件名最大长度 */
#define FILE_PATH_MAX_LEN  FILE_NAME_MAX_LEN/* 文件路径最大长度 */
#define FILE_LINE_MAX_LEN   (1024)          /* 文件行最大长度 */
#define IP_ADDR_MAX_LEN     (32)            /* IP地址最大长度 */
#define CMD_LINE_MAX_LEN    (1024)          /* 命令行最大长度 */
#define UDP_MAX_LEN         (1472)          /* UDP最大承载长度 */
#define QUEUE_NAME_MAX_LEN  (64)            /* 队列名最大长度 */
#define TABLE_NAME_MAX_LEN  (64)            /* 表名最大长度 */
#define ERR_MSG_MAX_LEN     (1024)          /* 错误信息最大长度 */
#define NODE_MAX_LEN        (64)            /* 节点名最大长度 */
#define INT_MAX_LEN         (128)           /* 整数字串的最大长度 */

#define MD5_SUM_CHK_LEN     (32)            /* MD5校验值长度 */

#define INVALID_FD          (-1)            /* 非法文件描述符 */
#define INVALID_PID         (-1)            /* 非法进程ID */

/* 进制 */
#define BIN                 (2)             /* 二进制 */
#define OCT                 (8)             /* 八进制 */
#define DEC                 (10)            /* 十进制 */
#define HEX                 (16)            /* 十六进制 */

#define ISPOWEROF2(x)    (0 == (((x)-1) & (x))) /* 判断x是否为2的n次方(2^n) */

/* 内存单位 */
#define KB                  (1024)          /* KB */
#define MB                  (1024 * KB)     /* MB */
#define GB                  (1024 * MB)     /* GB */

/* 获取较大值
 *  警告: 勿将MAX改为宏定义, 否则将会出现严重不可预测的问题
 *  原因: 如果出现多线程或多进程共享变量进行参与比较时, 可能出现比较时比较式成立,
 *        但返回时共享变量的值发生变化，导致返回结果不满足比较式. 这可能导致程序COREDUMP */
static inline int MAX(int a, int b) { return ((a) > (b) ? (a) : (b)); }
/* 获取较小值
 *  警告: 勿将MIN改为宏定义, 否则将会出现严重不可预测的问题
 *  原因: 如果出现多线程或多进程共享变量进行参与比较时, 可能出现比较时比较式成立,
 *        但返回时共享变量的值发生变化，导致返回结果不满足比较式. 这可能导致程序COREDUMP */
static inline int MIN(int a, int b) { return ((a) < (b) ? (a) : (b)); }

/* 将秒折算成: D天H时M分S秒 */
/* #define TM_DAY(sec)  ((sec) / (24*60*60)) */             /* 天 */
#define TM_DAY(sec)  ((sec) / (86400))                      /* 天 */
/* #define TM_HOUR(sec) (((sec) % (24*60*60))/(60*60)) */   /* 时 */
#define TM_HOUR(sec) (((sec) % (86400))/(3600))             /* 时 */
/* #define TM_MIN(sec)  ((((sec) % (24*60*60))%(60*60))/60) */ /* 分 */
#define TM_MIN(sec)  ((((sec) % (86400))%(3600))/60)        /* 分 */
/* #define TM_SEC(sec)  ((((sec) % (24*60*60))%(60*60))%60) */ /* 秒 */
#define TM_SEC(sec)  ((((sec) % (86400))%(3600))%60)        /* 秒 */

/* 内存对齐 */
#define mem_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define mem_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define PTR_ALIGNMENT   sizeof(unsigned long)

/* 变量成员在结构体中的偏移量 */
#define offsetof(type, field)   ((size_t)&(((type *)0)->field))

/******************************************************************************
 **函数名称: key_cb_t
 **功    能: 为唯一键产生KEY值
 **输入参数: 
 **     pkey: 主键(任意数据类型, 但该值必须是唯一的)
 **     pkey_len: 主键长度
 **输出参数: NONE
 **返    回: KEY值
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.11.09 #
 ******************************************************************************/
typedef int64_t (*key_cb_t)(const void *pkey, size_t pkey_len);
typedef int (*cmp_cb_t)(const void *data, const void *orig);
typedef int (*trav_cb_t)(void *data, void *args);

/******************************************************************************
 **函数名称: mem_alloc_cb_t
 **功    能: 分配内存回调类型
 **输入参数: 
 **     pool: 内存池
 **     size: 分配空间
 **输出参数:
 **返    回: 内存地址
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.26 #
 ******************************************************************************/
typedef void * (*mem_alloc_cb_t)(void *pool, size_t size);

void *mem_alloc(void *pool, size_t size);

/******************************************************************************
 **函数名称: mem_dealloc_cb_t
 **功    能: 回收内存回调类型
 **输入参数: 
 **     pool: 内存池
 **     p: 内存地址
 **输出参数:
 **返    回: VOID
 **实现描述: 
 **注意事项: 
 **作    者: # Qifeng.zou # 2014.12.26 #
 ******************************************************************************/
typedef void (*mem_dealloc_cb_t)(void *pool, void *p);

void mem_dealloc(void *pool, void *p);
static inline void mem_dummy_dealloc(void *pool, void *p) {};

/******************************************************************************
 **函数名称: key_cb_int32
 **功    能: 当主键为int32类型时的主键生成函数(外部接口)
 **输入参数:
 **     key: 主键
 **     len: 主键长度
 **输出参数: NONE
 **返    回: 主键
 **实现描述:
 **注意事项: 此时*key必须为int32类型
 **作    者: # Qifeng.zou # 2015.04.29 #
 ******************************************************************************/
static inline int key_cb_int32(const int *key, size_t len) { return *key; }
static inline int64_t key_cb_int64(const int64_t *key, size_t len) { return *key; }

/******************************************************************************
 **函数名称: cmp_cb_int32
 **功    能: 当主键为int类型时的主键比较函数(外部接口)
 **输入参数:
 **     key: 主键
 **     data: 与key值相等的键值对应的数据块
 **输出参数: NONE
 **返    回: 主键
 **实现描述:
 **注意事项: 因主键为int32类型, 因此调此函数时, 肯定返回相等
 **作    者: # Qifeng.zou # 2015.04.29 #
 ******************************************************************************/
static inline int cmp_cb_int32(const int *key, const void *data) { return 0; }
static inline int cmp_cb_int64(const int64_t *key, const void *data) { return 0; } 

/* 树操作接口 */
typedef int (*tree_insert_cb_t)(void *tree, void *key, int key_len, void *data);
typedef int (*tree_delete_cb_t)(void *tree, void *key, int key_len, void *data);
typedef int (*tree_query_cb_t)(void *tree, void *key, int key_len, void *data);
typedef void (*tree_trav_cb_t)(void *tree, trav_cb_t proc, void *args);
typedef void (*tree_destroy_cb_t)(void *tree);

#endif /*__COMM_H__*/
