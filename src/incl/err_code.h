#if !defined(__ERR_CODE_H__)
#define __ERR_CODE_H__

/* 错误码定义 */
typedef enum
{
    XDO_OK = 0                      /* 成功 */
    , XDO_AGAIN                     /* 再来一次 */
    , XDO_CONTINUE                  /* 继续执行 */
    , XDO_BREAK                     /* Break */

    , XDO_ERR_FAIL                  /* 失败 */
    , XDO_ERR_MEM_NOT_ENOUGH        /* 内存不足 */
    , XDO_ERR_SPACE_NOT_ENOUGH      /* 空间不足 */
    , XDO_ERR_PARAM_NOT_RIGHT       /* 参数错误 */

    /* 请在此行上方添加错误码 */
    , XDO_ERR_TOTAL
} err_code_e;

#endif /*__ERR_CODE_H__*/
