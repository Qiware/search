#if !defined(__HASH_H__)
#define __HASH_H__

#include "comm.h"

unsigned int hash_time33(const char *str);
unsigned int hash_time33_ex(const void *addr, size_t len);

#endif /*__HASH_H__*/
