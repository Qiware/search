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

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lcore

SRC_LIST = rtrd_recv.c \
			rtrd_comm.c \
			rtrd_lsn.c \
			rtrd_rsvr.c \
			rtrd_worker.c \
			rtrd_dist.c \
			rtsd_send.c \
			rtsd_ssvr.c \
			rtsd_worker.c

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
	@rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_LIB)/$(TARGET_A)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_LIB)/$(TARGET_A)"
