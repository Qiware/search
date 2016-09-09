#if !defined(__LOCK_H__)
#define __LOCK_H__

#include "comm.h"

/* 锁操作[注: 无须定义UNLOCK] */
typedef enum 
{
    NONLOCK             /* 不加锁 */
    , RDLOCK            /* 加读锁 */
    , WRLOCK            /* 加写锁 */
} lock_e;

int _flock(int fd, int type, int whence, int offset, int len);
int _try_flock(int fd, int type, int whence, int offset, int len);

/* 锁整个文件 */
#define proc_wrlock(fd) _flock(fd, F_WRLCK, SEEK_SET, 0, 0)  /* 写锁(全文件) */
#define proc_rdlock(fd) _flock(fd, F_RDLCK, SEEK_SET, 0, 0)  /* 读锁(全文件) */
#define proc_try_wrlock(fd) _try_flock(fd, F_WRLCK, SEEK_SET, 0, 0) /* 尝试写锁(全文件) */
#define proc_spin_wrlock(fd, idx) while (_try_flock(fd, F_WRLCK, SEEK_SET, 0, 0))
#define proc_try_rdlock(fd) _try_flock(fd, F_RDLCK, SEEK_SET, 0, 0) /* 尝试读锁(全文件) */
#define proc_spin_rdlock(fd, idx) while (_try_flock(fd, F_RDLCK, SEEK_SET, 0, 0))
#define proc_unlock(fd) _flock(fd, F_UNLCK, SEEK_SET, 0, 0)  /* 解锁(全文件) */

/* 锁文件指定区域 */
#define proc_wrlock_b(fd, idx) _flock(fd, F_WRLCK, SEEK_SET, idx, 1)   /* 写锁(单字节) */
#define proc_rdlock_b(fd, idx) _flock(fd, F_RDLCK, SEEK_SET, idx, 1)   /* 读锁(单字节) */
#define proc_try_wrlock_b(fd, idx) _try_flock(fd, F_WRLCK, SEEK_SET, idx, 1) /* 尝试写锁(单字节) */
#define proc_spin_wrlock_b(fd, idx) while (_try_flock(fd, F_WRLCK, SEEK_SET, idx, 1))
#define proc_try_rdlock_b(fd, idx) _try_flock(fd, F_RDLCK, SEEK_SET, idx, 1) /* 尝试读锁(单字节) */
#define proc_spin_rdlock_b(fd, idx) while (_try_flock(fd, F_RDLCK, SEEK_SET, idx, 1))
#define proc_unlock_b(fd, idx) _flock(fd, F_UNLCK, SEEK_SET, idx, 1)   /* 解锁(单字节) */

#endif /*__LOCK_H__*/
