###############################################################################
## Copyright(C) 2013-2014 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 实时消息队列(RTMQ)模块的编译
##         RTMQ: 实时消息队列
##         RTSD: 实时发送精灵
##         RTRD: 实时接收精灵
## 作  者: # Qifeng.zou # 2015.06.05 23:14:02 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
		  -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/incl/rtmq
INCLUDE += $(GLOBAL_INCLUDE)
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lcore
LIBS += $(SHARED_LIB)

SRC_LIST = ./server/rtmq_recv.c \
			./server/rtmq_comm.c \
			./server/rtmq_lsn.c \
			./server/rtmq_rsvr.c \
			./server/rtmq_worker.c \
			./server/rtmq_dist.c \
			./proxy/rtmq_proxy.c \
			./proxy/rtmq_proxy_ssvr.c \
			./proxy/rtmq_proxy_worker.c

OBJS = $(subst .c,.o, $(SRC_LIST))
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = librtmq.so
TARGET_A = librtmq.a

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

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr server/*.o proxy/*.o $(PROJ_LIB)/$(TARGET) $(PROJ_LIB)/$(TARGET_A)
	@echo "rm -fr server/*.o proxy/*.o $(PROJ_LIB)/$(TARGET) $(PROJ_LIB)/$(TARGET_A)"
