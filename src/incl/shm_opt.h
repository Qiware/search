#if !defined(__SHM_OPT_H__)
#define __SHM_OPT_H__

#include <sys/shm.h>
#include <sys/ipc.h>

key_t shm_ftok(const char *path, int id);
void *shm_creat(int key, size_t size);
void *shm_attach(int key, size_t size);
void *shm_creat_and_attach(int key, size_t size);

#endif /*__SHM_OPT_H__*/
