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

SRC_LIST = list.c \
			hash.c \
			list2.c \
			stack.c \
			queue.c \
			sha256.c \
			slab.c \
			shm_slab.c \
			xml_comm.c \
			xml_print.c \
			xml_tree.c \
			html_comm.c \
			html_print.c \
			html_tree.c \
			avl_tree.c \
			rb_tree.c \
			trie_tree.c \
			log.c \
			thread_pool.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libxdo_core.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	mv $@ $(PROJ)/lib
	rm -f $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	rm -fr $(OBJS) $(PROJ)/lib/$(TARGET) $(TARGET)
