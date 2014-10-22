#if !defined(__HASH_H__)
#define __HASH_H__

#include <stdint.h>

uint32_t hash_time33(const char *str);
uint32_t hash_time33_ex(const void *addr, size_t len);

#endif /*__HASH_H__*/
