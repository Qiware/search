/******************************************************************************
 ** Coypright(C) 2014-2024 Xundao technology Co., Ltd
 **
 ** 文件名: crawler.h
 ** 版本号: 1.0
 ** 描  述: 网络爬虫
 **         负责下载指定URL网页
 ** 作  者: # Qifeng.zou # 2014.09.04 #
 ******************************************************************************/
#if !defined(__CRAWLER_H__)
#define __CRAWLER_H__

typedef enum
{
    CRWL_OK = 0
    , CRWL_SHOW_HELP                        /* 显示帮助信息 */
    , CRWL_ERR = ~0x7fffffff                /* 失败、错误 */
} crwl_err_code_e;

/* 读取/发送快照 */
typedef struct
{
    int off;                                /* 偏移量 */
    int total;                              /* 总字节 */

    char *addr;                             /* 缓存首地址 */
} snap_shot_t;

#endif /*__CRAWLER_H__*/
