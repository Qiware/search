#if !defined(__HASH_ALG_H__)
#define __HASH_ALG_H__

#include "comm.h"

uint64_t hash_time33(const char *str);
uint64_t hash_time33_ex(const void *addr, size_t len);

#endif /*__HASH_ALG_H__*/
