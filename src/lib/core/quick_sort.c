/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: quick_sort.c
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年08月26日 星期五 17时13分16秒 #
 ******************************************************************************/

#include "comm.h"

typedef struct
{
    void *array;
    int len;

    sort_cmp_cb_t cmp;
    sort_swap_cb_t swap;
} quick_sort_t;

static int _quick_sort(quick_sort_t *sort, int low, int high);

/******************************************************************************
 **函数名称: quick_sort
 **功    能: 快速排序(外部接口)
 **输入参数:
 **     array: 数组
 **     len: 数组长度
 **     cmp: 比较函数
 **     swap: 交换函数
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.08.26 17:16:35 #
 ******************************************************************************/
int quick_sort(void *array, int len, sort_cmp_cb_t cmp, sort_swap_cb_t swap)
{
    quick_sort_t sort;

    if (len <= 0) {
        return 0;
    }

    sort.array = array;
    sort.len = len;
    sort.cmp = cmp;
    sort.swap = swap;

    return _quick_sort(&sort, 0, len-1);
}

/******************************************************************************
 **函数名称: _quick_sort_once
 **功    能: "一趟"快速排序
 **输入参数:
 **     sort: 排序对象
 **     low: 低位角标
 **     high: 高位角标
 **输出参数: NONE
 **返    回: 返回值M (注:M左边的比M右边的都小)
 **实现描述: 比较并交换位置, 使左边的数据比右边的小.
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.08.26 17:16:35 #
 ******************************************************************************/
static int _quick_sort_once(quick_sort_t *sort, int low, int high)
{
    int ret, i, j;

    i = low;
    j = high;
    while (i < j) {
        while (i < j) {
            ret = sort->cmp(sort->array, i, j);
            if (ret > 0) {
                sort->swap(sort->array, i, j);
                ++i;
                break;
            }
            --j;
        }

        while (i < j) {
            ret = sort->cmp(sort->array, i, j);
            if (ret > 0) {
                sort->swap(sort->array, i, j);
                --j;
                break;
            }
            ++i;
        }
    }

    return i;
}

/******************************************************************************
 **函数名称: _quick_sort
 **功    能: 快速排序
 **输入参数:
 **     sort: 排序对象
 **     low: 低位角标
 **     high: 高位角标
 **输出参数: NONE
 **返    回: 0:成功 !0:失败
 **实现描述:
 **注意事项: 
 **作    者: # Qifeng.zou # 2016.08.26 17:16:35 #
 ******************************************************************************/
static int _quick_sort(quick_sort_t *sort, int low, int high)
{
    int m;

    if (low < high) {
        m = _quick_sort_once(sort, low, high);

        _quick_sort(sort, low, m-1);
        _quick_sort(sort, m+1, high);
    }

    return 0;
}
