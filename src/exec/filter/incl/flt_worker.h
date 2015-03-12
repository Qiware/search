#if !defined(__FLT_WORKER_H__)
#define __FLT_WORKER_H__

#include "flt_priv.h"

/* 工作对象 */
typedef struct
{
    int tidx;
    flt_webpage_info_t info;               /* 网页信息 */
} flt_worker_t;

void *flt_worker_routine(void *_ctx);

bool flt_set_uri_exists(redis_cluster_t *cluster, const char *hash, const char *uri);
/* 判断uri是否已下载 */
#define flt_is_uri_down(cluster, hash, uri) flt_set_uri_exists(cluster, hash, uri)
/* 判断uri是否已推送 */
#define flt_is_uri_push(cluster, hash, uri) flt_set_uri_exists(cluster, hash, uri)

#endif /*__FLT_WORKER_H__*/
