#if !defined(__X_STRING_H__)
#define __X_STRING_H__

/* 字串 */
typedef struct
{
    char *str;      /* 字串值 */
    int len;        /* 字串长 */
} string_t;
 
string_t *str_to_low(string_t *s);

#endif /*__X_STRING_H__*/
