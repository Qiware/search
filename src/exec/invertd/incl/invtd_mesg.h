/******************************************************************************
 ** Coypright(C) 2014-2024 Qiware technology Co., Ltd
 **
 ** 文件名: invtd_mesg.h
 ** 版本号: 1.0
 ** 描  述: 
 ** 作  者: # Qifeng.zou # 2016年01月04日 星期一 20时57分36秒 #
 ******************************************************************************/
#if !defined(__INVTD_MESG_H__)
#define __INVTD_MESG_H__

int invtd_search_word_req_hdl(int type, int dev_orig, char *buff, size_t len, void *args);
int invtd_insert_word_req_hdl(int type, int dev_orig, char *buff, size_t len, void *args);

#endif /*__INVTD_MESG_H__*/
