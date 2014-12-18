#if !defined(__SPIN_LOCK_H__)
#define __SPIN_LOCK_H__

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/syscall.h>

/* 自旋锁 */
typedef struct
{
    unsigned short ticket;
    unsigned short owner;
} spinlock_t;

/* 初始化 */
#define spin_lock_init(lock) \
{ \
    do { \
        (lock)->ticket = 0; \
        (lock)->owner = 0; \
    } while(0); \
}

/* 加锁 */
#define spin_lock(lock) \
{ \
    unsigned short ticket = 0x01; \
    __asm__ __volatile__ ( \
        "lock xaddw %0, %1\n"   /* swap(ticket,lock->ticket); lock->ticket += ticket; */\
        "1:\n"                  /* RECHECK: */\
        "cmpw %0, %2\n"         /* if (ticket == owner) */\
        "je 2f\n"               /*   goto QUIT; */\
        "jmp 1b\n"              /* goto RECHECK; */\
        "2:"                    /* QUIT: */\
        :"+q" (ticket), "+m" ((lock)->ticket), "+m" ((lock)->owner) \
        : \
        :"memory", "cc"); \
}

/* 解锁 */
#define spin_unlock(lock) \
{ \
    __asm__ __volatile__( \
        "lock incw %0" \
        :"+m" ((lock)->owner) \
        : \
        :"memory", "cc"); \
}

/* 销毁 */
#define spin_lock_destroy(lock) spin_lock_init(lock)

#endif /*__SPIN_LOCK_H__*/
