#if !defined(__INVERT_H__)
#define __INVERT_H__

#include "comm.h"
#include "list.h"
#include "btree.h"

/* 倒排文件 */
typedef struct
{
    int path[FILE_LINE_MAX_LEN];    /* 倒排文件路径 */
} invert_file_t;

/* 倒排索引 */
typedef struct
{
} invert_index_t;

/* 倒排对象 */
typedef struct
{
    int max;                        /* 哈希数组大小 */
    int fd;                         /* 文件描述符 */
    char path[FILE_LINE_MAX_LEN];   /* 倒排索引文件路径 */
} invert_cntx_t;

/* 对外接口 */
invert_cntx_t *invert_creat(const char *path, int max);
int invert_insert(const char *keyword, const char *doc);
int invert_query(const char *keyword, list_t *list);
int invert_remove(const char *keyword);

#endif /*__INVERT_H__*/
