#if !defined(__TICKET_LOCK_H__)
#define __TICKET_LOCK_H__

/* 自旋锁 */
typedef struct
{
    uint16_t ticket;
    uint16_t owner;
} ticketlock_t;

/* 初始化 */
static inline void ticket_lock_init(ticketlock_t *lock)
{
    (lock)->ticket = 0;
    (lock)->owner = 0;
}

/* 加锁 */
static inline void ticket_lock(ticketlock_t *lock)
{
    uint16_t ticket = 0x01;

    __asm__ __volatile__ (
        /* 取票: 每张票有唯一的编号
         * lock: 锁定操作数的内存地址
         * xddw: 两边操作数交换后 再相加 */
        "lock xaddw %0, %1\n"   /* swap(ticket, lock->ticket); lock->ticket += ticket; */
        /* 跳转标记: RECHECK */
        "1:\n"                  /* RECHECK: */
        /* 比较票的编号是否与下一个票的拥有者一致 */
        "cmpw %0, %2\n"         /* if (ticket == owner) */
        /* 如果相等, 则退出 */
        "je 2f\n"               /* goto QUIT; */
        /* 如果不等, 则继续比较 */
        "jmp 1b\n"              /* goto RECHECK; */
        /* 跳转标记: 退出 */
        "2:"                    /* QUIT: */
        :"+q" (ticket), "+m" ((lock)->ticket), "+m" ((lock)->owner)
        :
        :"memory", "cc");
}

/* 解锁 */
static inline void ticket_unlock(ticketlock_t *lock)
{
    __asm__ __volatile__(
        "lock incw %0"
        :"+m" ((lock)->owner)
        :
        :"memory", "cc");
}

/* 销毁 */
#define ticket_lock_destroy(lock) ticket_lock_init(lock)

#endif /*__TICKET_LOCK_H__*/
