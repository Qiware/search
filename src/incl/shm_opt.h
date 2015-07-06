#if !defined(__SHM_OPT_H__)
#define __SHM_OPT_H__

#include <sys/shm.h>
#include <sys/ipc.h>

/* SHM数据 */
typedef struct
{
    int id;                     /* ID */
    size_t size;                /* 空间 */
} shm_data_t;

#define SHM_DATA_INVALID(shm)   /* 验证合法性 */\
    ((0 == (shm)->size) || (0 == (shm)->id) || (-1 == (shm)->id))

key_t shm_ftok(const char *path, int id);
void *shm_creat_by_key(int key, size_t size);
void *shm_attach_by_key(key_t key, size_t size);

void *shm_creat(const char *path, size_t size);
void *shm_attach(const char *path, size_t size);

#endif /*__SHM_OPT_H__*/
