#if !defined(__LSND_MESG_H__)
#define __LSND_MESG_H__

int lsnd_search_word_req_hdl(unsigned int type, void *data, int length, void *args);
int lsnd_search_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

int lsnd_insert_word_req_hdl(unsigned int type, void *data, int length, void *args);
int lsnd_insert_word_rsp_hdl(int type, int orig, char *data, size_t len, void *args);

#endif /*__LSND_MESG_H__*/
