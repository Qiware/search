#if !defined(__LWSD_MESG_H__)
#define __LWSD_MESG_H__

int lwsd_search_req_hdl(unsigned int type, void *data, int length, void *args);
int lwsd_search_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

int lwsd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args);
int lwsd_insert_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

#endif /*__LWSD_MESG_H__*/
