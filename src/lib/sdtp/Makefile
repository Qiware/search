###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 通用消息传输协议接收模块的编译
## 作  者: # Qifeng.zou # 2014.12.29 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
		  -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/incl/sdtp

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lcore

SRC_LIST = sdrd_recv.c \
			sdrd_comm.c \
			sdrd_lsn.c \
			sdrd_rsvr.c \
			sdrd_worker.c \
			sdrd_cli.c \
			sdsd_send.c \
			sdsd_pool.c \
			sdsd_ssvr.c \
			sdsd_worker.c \
			sdsd_cli.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = libsdtp.so
TARGET_A = libsdtp.a

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
	@rm -fr *.o $(PROJ_LIB)/$(TARGET) $(OBJS)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(OBJS)"
