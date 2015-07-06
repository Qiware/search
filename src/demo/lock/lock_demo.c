#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//static __always_inline void __ticket_spin_lock(arch_spinlock_t *lock) {
//    short inc = 0x0100;
//    asm volatile (
//            /* 对SMP内核来说，LOCK_PREFIX为”\n\tlock”
//               Lock是一个指令前缀，表示在接下来的一个指令内，
//               LOCK信号被ASSERT，指令所访问的内存区域将为独占访问。
//               具体实现或是BUS锁定，或是Cache一致性操作。
//               可参考intel system program guide 8.1 另：这一实现是最新的实现，
//               名为ticket实现，即每个希望获得锁的代码都会得到一张ticket，
//               ticket按顺序增长，锁内部会维护一个当前使用锁的ticket号owner，
//               和下一个使用锁的ticket号next，各一个字节。
//               当锁处于释放状态时，owner=next，如果锁处于锁定状态，则next=owner+1。
//               获得锁的时候，将next+1，释放锁的时候将owner+1。*/
//            LOCK_PREFIX "xaddw %w0, %1\n"
//            "1:\t"
//            "cmpb %h0, %b0\n\t"
//            "je 2f\n\t"
//            "rep ; nop\n\t"
//            "movb %1, %b0\n\t"
//            /* don't need lfence here, because loads are in-order */
//            "jmp 1b\n"
//            "2:"
//            : "+Q" (inc), "+m" (lock->slock) :
//                : "memory", "cc");
//}

#if 1
int main(void)
{
    free(NULL);
    return 0;
}
#else
#include <stdio.h>
#include <pthread.h>
#include <alsa/iatomic.h>

// 定义一个原子变量
static atomic_t g_atomic = ATOMIC_INIT(1);
// 定义共享资源
static volatile int g_i = 0;

/* 定义线程处理函数 */
//#define atomic_dec_and_test(g_atomic) 1 
void *thr1_handle(void *arg)
{
    while (1) {
        if (atomic_dec_and_test(&g_atomic)) {
            printf("in thread %lu g_i = %d\n", pthread_self(), ++g_i);
        }
        atomic_inc(&g_atomic);
        sleep(1);
    }

    return NULL; 
}

void *thr2_handle(void *arg)
{
    while (1) {
        if (atomic_dec_and_test(&g_atomic)) {
            printf("in thread %lu g_i = %d\n", pthread_self(), --g_i);
        }
        atomic_inc(&g_atomic);
        sleep(1);
    }
    return NULL; 
}


/* 主函数 */
int main(void)
{
    pthread_t tid1, tid2;
    if (pthread_create(&tid1, NULL, thr1_handle, NULL) != 0) {
        fprintf(stderr, "create thread1 failed!\n");
        return 1;
    }
    if (pthread_create(&tid2, NULL, thr2_handle, NULL) != 0) {
        fprintf(stderr, "create thread2 failed!\n");
        return 2;
    }

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    return 0;
}
#endif
