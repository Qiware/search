#if !defined(__KMP_H__)
#define __KMP_H__

#include "comm.h"

typedef struct
{
    char *p;                            /* 模式串 */
    int len;                            /* 模式串长度 */

    int *next;                          /* NEXT数组 */
} kmp_t;

kmp_t *kmp_creat(char *p, int len);
int kmp_match(kmp_t *kmp, const char *data, int len);
void kmp_print(kmp_t *kmp);
void kmp_destroy(kmp_t *kmp);

#endif /*__KMP_H__*/
