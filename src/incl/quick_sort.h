/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: quick_sort.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年08月26日 星期五 17时40分01秒 #
 ******************************************************************************/
#if !defined(__QUICK_SORT__)
#define __QUICK_SORT__

#include "comm.h"

void quick_sort(void *array, int len, sort_cmp_cb_t cmp, sort_swap_cb_t swap);

#endif /*__QUICK_SORT__*/
