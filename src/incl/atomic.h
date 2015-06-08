#if !defined(__ATOMIC_H__)
#define __ATOMIC_H__

#include <stdint.h>

/******************************************************************************
 **函数名称: atomic16_xset
 **功    能: 先返回v中的值，再执行 (*v) = i
 **输入参数: 
 **     i: 目标值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的原有值
 **实现描述: 
 **注意事项: 其过程是原子操作
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_xset(volatile uint16_t *v, uint16_t i)
{
    __asm__ volatile (
            "lock xchg %0, %1"
            :"+r" (i), "+m" (*v)
            :
            :"memory");

    return i; /* i此时值等于v的原始值 */
}

static inline uint32_t atomic32_xset(volatile uint32_t *v, uint32_t i)
{
    __asm__ volatile (
        "lock xchgl %0, %1"
        :"+r" (i), "+m" (*v)
        :
        :"memory");

    return i; /* i此时值等于v的原始值 */
}

static inline uint64_t atomic64_xset(volatile uint64_t *v, uint64_t i)
{
    __asm__ volatile (
        "lock xchgq %0, %1"
        :"+r" (i), "+m" (*v)
        :
        :"memory");

    return i; /* i此时值等于v的原始值 */
}

/******************************************************************************
 **函数名称: atomic16_xadd
 **功    能: 先返回v中的值，再执行(*v) += i
 **输入参数: 
 **     i: 增量值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的原始值
 **实现描述: 
 **注意事项: 其过程是原子操作
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_xadd(volatile uint16_t *v, uint16_t i)
{
    __asm__ volatile (
            "lock xaddw %0, %1"   // swap(i, v); v += i;
            :"+r" (i), "+m" (*v)
            :
            :"memory");

    return i; /* i此时值等于v的原始值 */
}

static inline uint32_t atomic32_xadd(volatile uint32_t *v, uint32_t i)
{
    __asm__ volatile (
        "lock xaddl %0, %1"   // swap(i, v); v += i;
        :"+r" (i), "+m" (*v)
        :
        :"memory");

    return i; /* i此时值等于v的原始值 */
}

static inline uint64_t atomic64_xadd(volatile uint64_t *v, uint64_t i)
{
    __asm__ volatile (
        "lock xaddq %0, %1"   // swap(i, v); v += i;
        :"+r" (i), "+m" (*v)
        :
        :"memory");

    return i; /* i此时值等于v的原始值 */
}

/******************************************************************************
 **函数名称: atomic16_xsub
 **功    能: 先返回v中的值，再执行(*v) -= i
 **输入参数: 
 **     i: 目标值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的原有值
 **实现描述: 
 **注意事项: 其过程是原子操作
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_xsub(volatile uint16_t *v, uint16_t i)
{
    return atomic16_xadd(v, -i);
}

static inline uint32_t atomic32_xsub(volatile uint32_t *v, uint32_t i)
{
    return atomic32_xadd(v, -i);
}

static inline uint64_t atomic64_xsub(volatile uint64_t *v, uint64_t i)
{
    return atomic64_xadd(v, -i);
}

/******************************************************************************
 **函数名称: atomic16_xinc
 **功    能: 先返回v中的值，再执行(*v) += 1
 **输入参数: NONE
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的原有值
 **实现描述: 
 **注意事项: 其过程是原子操作
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_xinc(volatile uint16_t *v)
{
    return atomic16_xadd(v, 1);
}

static inline uint32_t atomic32_xinc(volatile uint32_t *v)
{
    return atomic32_xadd(v, 1);
}

static inline uint64_t atomic64_xinc(volatile uint64_t *v)
{
    return atomic64_xadd(v, 1);
}

/******************************************************************************
 **函数名称: atomic16_xdec
 **功    能: 先返回v中的值，再执行(*v) -= 1
 **输入参数: NONE
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的原有值
 **实现描述: 
 **注意事项: 其过程是原子操作
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_xdec(volatile uint16_t *v)
{
    return atomic16_xsub(v, 1);
}

static inline uint32_t atomic32_xdec(volatile uint32_t *v)
{
    return atomic32_xsub(v, 1);
}

static inline uint64_t atomic64_xdec(volatile uint64_t *v)
{
    return atomic64_xsub(v, 1);
}

/******************************************************************************
 **函数名称: atomic16_set
 **功    能: 先执行(*v) = i，再返回 (*v) 的值
 **输入参数:
 **     i: 目标值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的新值
 **实现描述: 
 **注意事项: 勿执行return (*v)  原因: 否则非原子
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_set(volatile uint16_t *v, uint64_t i)
{
    atomic16_xset(v, i);

    return i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

static inline uint32_t atomic32_set(volatile uint32_t *v, uint32_t i)
{
    atomic32_xset(v, i);

    return i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

static inline uint64_t atomic64_set(volatile uint64_t *v, uint64_t i)
{
    atomic64_xset(v, i);

    return i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

/******************************************************************************
 **函数名称: atomic16_add
 **功    能: 先执行(*v) += i，再返回 (*v) 的值
 **输入参数:
 **     i: 增量值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的新值
 **实现描述: 
 **注意事项: 勿执行return (*v)  原因: 否则非原子
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_add(volatile uint16_t *v, uint16_t i)
{
    return atomic16_xadd(v, i) + i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

static inline uint32_t atomic32_add(volatile uint32_t *v, uint32_t i)
{
    return atomic32_xadd(v, i) + i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

static inline uint64_t atomic64_add(volatile uint64_t *v, uint64_t i)
{
    return atomic64_xadd(v, i) + i; /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

/******************************************************************************
 **函数名称: atomic16_sub
 **功    能: 先执行(*v) -= i，再返回 (*v) 的值
 **输入参数:
 **     i: 减量值
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的新值
 **实现描述: 
 **注意事项: 勿执行return (*v)  原因: 否则非原子
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_sub(volatile uint16_t *v, uint16_t i)
{
    return atomic16_xsub(v, i) - i; /* 勿直接return (*v)  原因: 否则非原子 */
}

static inline uint32_t atomic32_sub(volatile uint32_t *v, uint32_t i)
{
    return atomic32_xsub(v, i) - i; /* 勿直接return (*v)  原因: 否则非原子 */
}

static inline uint64_t atomic64_sub(volatile uint64_t *v, uint64_t i)
{
    return atomic64_xsub(v, i) - i; /* 勿直接return (*v)  原因: 否则非原子 */
}

/******************************************************************************
 **函数名称: atomic16_inc
 **功    能: 先执行(*v) += 1，再返回 (*v) 的值
 **输入参数: NONE
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的新值
 **实现描述: 
 **注意事项: 勿执行return (*v)  原因: 否则非原子
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_inc(volatile uint16_t *v)
{
    return atomic16_add(v, 1); /* 勿直接return (*v)  原因: 否则非原子 */
}

static inline uint32_t atomic32_inc(volatile uint32_t *v)
{
    return atomic32_add(v, 1); /* 勿直接return (*v)  原因: 否则非原子 */
}

static inline uint64_t atomic64_inc(volatile uint64_t *v)
{
    return atomic64_add(v, 1); /* 勿直接return (*v)  原因: 否则非原子 */
}

/******************************************************************************
 **函数名称: atomic16_dec
 **功    能: 先执行(*v) -= 1，再返回 (*v) 的值
 **输入参数: NONE
 **输出参数:
 **     v: 被操作变量
 **返    回: 变量v的新值
 **实现描述: 
 **注意事项: 勿执行return (*v)  原因: 否则非原子
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline uint16_t atomic16_dec(volatile uint16_t *v)
{
    return atomic16_sub(v, 1); /* 勿直接执行 return (*v);  原因: 否则非原子 */
}

static inline uint32_t atomic32_dec(volatile uint32_t *v)
{
    return atomic32_sub(v, 1); /* 勿直接return (*v)  原因: 否则非原子 */
}

static inline uint64_t atomic64_dec(volatile uint64_t *v)
{
    return atomic64_sub(v, 1); /* 勿直接return (*v)  原因: 否则非原子 */
}

/******************************************************************************
 **函数名称: atomic16_cmp_and_set
 **功    能: 如果(*v) == cmp, 则执行(*v) = i, 并返回true; 否则, 返回false!
 **输入参数:
 **     cmp: 比较值
 **     i: 目标值
 **输出参数:
 **     v: 被操作变量
 **返    回: 改变返回TRUE, 未改返回FALSE
 **实现描述: 
 **     if (*v == cmp) {
 **         *v = i;
 **         return 1;
 **     }
 **     else {
 **         return 0;
 **     }
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.18 #
 ******************************************************************************/
static inline int atomic16_cmp_and_set(volatile uint16_t *v, uint16_t cmp, uint16_t i)
{
	uint8_t res;

	__asm__ volatile(
			"lock cmpxchgw %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*v)
			: [src] "r" (i),        /* input */
			  "a" (cmp),
			  "m" (*v)
			: "memory");            /* no-clobber list */
	return (int)res;
}

static inline int atomic32_cmp_and_set(volatile uint32_t *v, uint32_t cmp, uint32_t i)
{
	uint8_t res;

	__asm__ volatile(
			"lock cmpxchgl %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*v)
			: [src] "r" (i),        /* input */
			  "a" (cmp),
			  "m" (*v)
			: "memory");            /* no-clobber list */
	return (int)res;
}

static inline int atomic64_cmp_and_set(volatile uint64_t *v, uint64_t cmp, uint64_t i)
{
	uint8_t res;

	asm volatile(
			"lock cmpxchgq %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*v)
			: [src] "r" (i),        /* input */
			  "a" (cmp),
			  "m" (*v)
			: "memory");            /* no-clobber list */

	return (int)res;
}
#endif /*__ATOMIC_H__*/
