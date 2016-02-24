#if !defined(__AGENT_MESG_H__)
#define __AGENT_MESG_H__

#include <stdint.h>

/* 报头 */
typedef struct
{
    uint32_t type;                      /* 消息类型 Range: mesg_type_e */
#define AGENT_MSG_FLAG_SYS   (0)        /* 0: 系统数据类型 */
#define AGENT_MSG_FLAG_USR   (1)        /* 1: 自定义数据类型 */
    uint32_t flag;                      /* 标识量(0:系统数据类型 1:自定义数据类型) */
    uint32_t length;                    /* 报体长度 */
#define AGENT_MSG_MARK_KEY   (0x1ED23CB4)
    uint32_t mark;                      /* 校验值 */
    uint32_t from;                      /* 源设备ID */
    uint32_t to;                        /* 目标设备ID */

    uint64_t serial;                    /* 流水号 */
} agent_header_t;

/* 字节序转换 */
#define agent_head_hton(h, n) do { /* 主机->网络 */\
    (n)->type = htonl((h)->type); \
    (n)->flag = htonl((h)->flag); \
    (n)->length = htonl((h)->length); \
    (n)->mark = htonl((h)->mark); \
    (n)->serial = hton64((h)->serial); \
} while(0)

#define agent_head_ntoh(n, h) do { /* 网络->主机*/\
    (h)->type = ntohl((n)->type); \
    (h)->flag = ntohl((n)->flag); \
    (h)->length = ntohl((n)->length); \
    (h)->mark = ntohl((n)->mark); \
    (h)->serial = ntoh64((n)->serial); \
} while(0)

#endif /*__AGENT_MESG_H__*/
