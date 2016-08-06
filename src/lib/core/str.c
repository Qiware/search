#include "str.h"
#include "comm.h"

/******************************************************************************
 **函数名称: str_to_lower
 **功    能: 将字串转为小字母
 **输入参数:
 **     str: 字串
 **输出参数:
 **返    回: 字串对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
str_t *str_to_lower(str_t *s)
{
    size_t idx;

    for (idx=0; idx<s->len; ++idx) {
        switch (s->str[idx]) {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            {
                s->str[idx] += 32;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return s;
}

/******************************************************************************
 **函数名称: char_to_lower
 **功    能: 将字串转为小字母
 **输入参数:
 **     str: 字串(源)
 **     dstr: 目的字串
 **     len: 指定长度
 **输出参数:
 **返    回: 字串对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
char *char_to_lower(const char *str, char *dstr, int len)
{
    int idx;

    for (idx=0; idx<len && '\0'!=str[idx]; ++idx) {
        switch (str[idx]) {
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            {
                dstr[idx] = str[idx] + 32;
                break;
            }
            default:
            {
                dstr[idx] = str[idx];
                break;
            }
        }
    }

    dstr[idx] = '\0';

    return dstr;
}



/******************************************************************************
 **函数名称: str_to_upper
 **功    能: 将字串转为大字母
 **输入参数:
 **     str: 字串
 **输出参数:
 **返    回: 字串对象
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2014.09.18 #
 ******************************************************************************/
str_t *str_to_upper(str_t *s)
{
    size_t idx;

    for (idx=0; idx<s->len; ++idx) {
        switch (s->str[idx]) {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            {
                s->str[idx] -= 32;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return s;
}

/******************************************************************************
 **函数名称: str_isdigit
 **功    能: 字串是否为数字
 **输入参数:
 **     str: 字串
 **输出参数:
 **返    回: true:是 false:否
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015.04.25 #
 ******************************************************************************/
bool str_isdigit(const char *str)
{
    const char *p = str;

    for (;'\0' != *p; ++p) {
        if (!isdigit(*p)) {
            return false;
        }
    }
    return true;
}

/******************************************************************************
 **函数名称: str_to_hex
 **功    能: 将十六进制字符串转换为十六进制数据
 **输入参数:
 **     str: 源字串
 **     len: 源字串长度
 **输出参数:
 **     hex: 十六进制数据
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项:
 **作    者: # Qifeng.zou # 2015-06-03 11:45:50 #
 ******************************************************************************/
int str_to_hex(const char *str, int len, char *hex)
{
    int i, j;

    for (i=0; i<len; ++i) {
        j = i>>1;
        if (str[i] >= 'A' && str[i] <= 'F') {
            if (0 == j) {
                hex[j] = (str[i]-'A'+10)<<4;
            }
            else {
                hex[j] |= str[i]-'A'+10;
            }
            continue;
        }
        else if (str[i] >= 'a' && str[i] <= 'f') {
            if (0 == j) {
                hex[j] = (str[i]-'a'+10)<<4;
            }
            else {
                hex[j] |= str[i]-'a'+10;
            }
            continue;
        }
        else if (str[i] >= '0' && str[i] <= '9') {
            if (0 == j) {
                hex[j] = (str[i]-'0')<<4;
            }
            else {
                hex[j] |= (str[i]-'0');
            }
            continue;
        }
        return -1;
    }

    return 0;
}

/******************************************************************************
 **函数名称: str_to_num
 **功    能: 将字串转换为数字.
 **输入参数:
 **     str: 字符串
 **输出参数: NONE
 **返    回: SIZE
 **实现描述:
 **注意事项:
 **     > 1K = 1000
 **     > 1M = 1000000
 **     > 1G = 1000000000
 **     > 1Kb = 1024(byte)
 **     > 1Mb = 1024 * Kb(byte)
 **     > 1Gb = 1024 * Mb(byte)
 **作    者: # Qifeng.zou # 2016.08.02 22:05:12 #
 ******************************************************************************/
size_t str_to_num(const char *str)
{
    int len = 0;
    size_t num = 0, unit;
    const char *ptr = str;

    for (; '\0'!=*ptr; ptr+=1) {
        if (isdigit(*ptr)) {
            num = 10*num + (*ptr - '0');
            continue;
        }
        else if (' ' == *ptr) {
            continue;
        }

        /* 比较单位 */
        if (!strncasecmp(ptr, "KB", 2)) {
            len = 2;
            unit = KB;
            break;
        }
        else if (!strncasecmp(ptr, "MB", 2)) {
            len = 2;
            unit = MB;
            break;
        }
        else if (!strncasecmp(ptr, "GB", 2)) {
            len = 2;
            unit = GB;
            break;
        }
        else if (!strncasecmp(ptr, "K", 1)) {
            len = 1;
            unit = K;
            break;
        }
        else if (!strncasecmp(ptr, "M", 1)) {
            len = 1;
            unit = M;
            break;
        }
        else if (!strncasecmp(ptr, "G", 1)) {
            len = 1;
            unit = G;
            break;
        }
        return 0;
    }

    if (len > 0) {
        ptr += len;
        for (; '\0'!=*ptr; ptr+=1) {
            if (' ' != *ptr) {
                return 0;
            }
        }
        return num * unit;
    }

    return num;
}
