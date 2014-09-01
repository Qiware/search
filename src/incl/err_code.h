#if !defined(__ERR_CODE_H__)
#define __ERR_CODE_H__

/* 错误码定义 */
typedef enum
{
    XDT_OK = 0                      /* 成功 */
    , XDT_AGAIN                     /* 再来一次 */
    , XDT_CONTINUE                  /* 继续执行 */
    , XDT_BREAK                     /* Break */

    , XDT_ERR_FAIL                  /* 失败 */
    , XDT_ERR_MEM_NOT_ENOUGH        /* 内存不足 */
    , XDT_ERR_SPACE_NOT_ENOUGH      /* 空间不足 */
    , XDT_ERR_PARAM_NOT_RIGHT       /* 参数错误 */

    /* 请在此行上方添加错误码 */
    , XDT_ERR_TOTAL
}err_code_e;

#endif /*__ERR_CODE_H__*/
