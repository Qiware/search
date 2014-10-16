###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 核心模块的编译
## 作  者: # Qifeng.zou # 2014.08.28 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread

SRC_LIST = hash.c \
			log.c \
			log2.c \
			lock.c \
			list.c \
			list2.c \
			queue.c \
			sha256.c \
			slab.c \
			syscall.c \
			shm_slab.c \
			mem_pool.c \
			shm_queue.c \
			rb_tree.c \
			avl_tree.c \
			trie_tree.c \
			xml_comm.c \
			xml_print.c \
			xml_tree.c \
			html_comm.c \
			html_print.c \
			html_tree.c \
			thread_pool.c \
			tcp_socket.c \
			unix_socket.c \
			x_string.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libxd_core.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"
	@rm -fr $(OBJS)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(PROJ_LIB)/$(TARGET)
