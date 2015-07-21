###############################################################################
## Copyright(C) 2013-2014 Xundao technology Co., Ltd
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
			plog.c \
			slot.c \
			lock.c \
			http.c \
			list.c \
			list2.c \
			md5.c \
			menu.c \
			ring.c \
			queue.c \
			sha256.c \
			slab.c \
			redo.c \
			shm_slab.c \
			shm_opt.c \
			mem_pool.c \
			shm_ring.c \
			shm_slot.c \
			shm_queue.c \
			btree.c \
			rb_tree.c \
			kw_tree.c \
			avl_tree.c \
			xml_comm.c \
			xml_print.c \
			xml_tree.c \
			thread_pool.c \
			sck_tcp.c \
			sck_udp.c \
			sck_unix.c \
			hash_tab.c \
			str.c \
			uri.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = libcore.so
TARGET_A = libcore.a

.PHONY: all clean

all: $(TARGET) $(TARGET_A)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(TARGET_A): $(OBJS)
	@$(AR) $(AFLAGS) -s $@ $(OBJS)
	@echo "AR $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET)"
