#if !defined(__SPIN_LOCK_H__)
#define __SPIN_LOCK_H__

#include "comm.h"
#include "atomic.h"

#define SPIN_LOCK_LOCKED  (0)   /* 已锁 */
#define SPIN_LOCK_UNLOCK  (1)   /* 未锁 */

/* 自旋锁 */
typedef struct
{
    uint16_t l;
} spinlock_t;

/******************************************************************************
 **函数名称: spin_lock_init
 **功    能: 初始化自旋锁
 **输入参数:
 **     lck: 自旋锁
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline void spin_lock_init(spinlock_t *lck)
{
    lck->l = SPIN_LOCK_UNLOCK;
}

/******************************************************************************
 **函数名称: spin_lock
 **功    能: 加锁
 **输入参数:
 **     lck: 自旋锁
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline void spin_lock(spinlock_t *lck)
{
    do {} while(!atomic16_cmp_and_set(&lck->l, SPIN_LOCK_UNLOCK, SPIN_LOCK_LOCKED));
}

/******************************************************************************
 **函数名称: spin_trylock
 **功    能: 尝试加锁
 **输入参数:
 **     lck: 自旋锁
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline int spin_trylock(spinlock_t *lck)
{
    return atomic16_cmp_and_set(&lck->l, SPIN_LOCK_UNLOCK, SPIN_LOCK_LOCKED)? 0 : -1;
}

/******************************************************************************
 **函数名称: spin_unlock
 **功    能: 解锁
 **输入参数:
 **     lck: 自旋锁
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline int spin_unlock(spinlock_t *lck)
{
    return atomic16_cmp_and_set(&lck->l, SPIN_LOCK_LOCKED, SPIN_LOCK_UNLOCK)? 0 : -1;
}

/******************************************************************************
 **函数名称: spin_lock_destroy
 **功    能: 销毁自旋锁
 **输入参数:
 **     lck: 自旋锁
 **输出参数: NONE
 **返    回: VOID
 **实现描述: 
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline void spin_lock_destroy(spinlock_t *lck)
{
    lck->l = SPIN_LOCK_UNLOCK;
}
#endif /* __SPIN_LOCK_H__ */
