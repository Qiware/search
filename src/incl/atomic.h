#if !defined(__ATOMIC_H__)
#define __ATOMIC_H__

#include <stdint.h>

typedef uint64_t atomic_t;

#define LOCK_PREFIX_ATOMIC \
    ".section .smp_locks,\"a\"\n"	\
    "  .align 8\n"			\
    "  .quad 661f\n"	\
    ".previous\n"			\
    "661:\n\tlock; "


static inline void atomic64_add(unsigned long volatile *v, unsigned long i)
{
	__asm__ __volatile__(
		LOCK_PREFIX_ATOMIC "addq %1,%0"
		:"=m" (*v)
		:"ir" (i), "m" (*v));
}

/* 自加1 */
static inline void atomic64_inc(unsigned long volatile *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX_ATOMIC "incq %0"
		:"=m" (*v)
		:"m" (*v));
}

/* 自减1 */
static inline void atomic64_dec(unsigned long volatile *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX_ATOMIC "decq %0"
		:"=m" (*v)
		:"m" (*v));
}
#endif /*__ATOMIC_H__*/
