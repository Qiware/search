#if !defined(__ERR_CODE_H__)
#define __ERR_CODE_H__

/* 错误码定义 */
typedef enum
{
    XDS_OK = 0                      /* 成功 */
    , XDS_AGAIN                     /* 再来一次 */
    , XDS_CONTINUE                  /* 继续执行 */
    , XDS_BREAK                     /* Break */

    , XDS_ERR                       /* 失败 */
    , XDS_ERR_MEM_NOT_ENOUGH        /* 内存不足 */
    , XDS_ERR_SPACE_NOT_ENOUGH      /* 空间不足 */
    , XDS_ERR_PARAM_NOT_RIGHT       /* 参数错误 */

    /* 请在此行上方添加错误码 */
    , XDS_ERR_TOTAL
} err_code_e;

#endif /*__ERR_CODE_H__*/
