#if !defined(__STR_H__)
#define __STR_H__

#include <stdbool.h>

/* 字串 */
typedef struct
{
    char *str;      /* 字串值 */
    int len;        /* 字串长 */
} str_t;
 
str_t *str_to_lower(str_t *s);
str_t *str_to_upper(str_t *s);
bool str_isdigit(const char *str);

#endif /*__STR_H__*/
