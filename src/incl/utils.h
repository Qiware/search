/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: utils.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年06月15日 星期三 00时17分23秒 #
 ******************************************************************************/
#if !defined(__UTILS_H__)
#define __UTILS_H__

uint64_t tlz_gen_sid(uint16_t nid, uint16_t svrid, uint32_t seq);
uint64_t tlz_gen_serail(uint16_t nid, uint16_t sid, uint32_t seq);

#endif /*__UTILS_H__*/
