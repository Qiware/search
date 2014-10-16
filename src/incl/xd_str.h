#if !defined(__XD_STR_H__)
#define __XD_STR_H__

/* 字串 */
typedef struct
{
    char *str;      /* 字串值 */
    int len;        /* 字串长 */
} xd_str_t;
 
xd_str_t *str_to_lower(xd_str_t *s);
xd_str_t *str_to_upper(xd_str_t *s);

#endif /*__XD_STR_H__*/
