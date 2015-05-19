###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 通用消息传输协议接收模块的编译
## 作  者: # Qifeng.zou # 2014.12.29 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_core

SRC_LIST = recv/sdtp_recv.c \
			recv/sdtp_comm.c \
			recv/sdtp_rlsn.c \
			recv/sdtp_rsvr.c \
			recv/sdtp_rwrk.c \
			send/sdtp_send.c \
			send/sdtp_pool.c \
			send/sdtp_ssvr.c \
			send/sdtp_swrk.c \
			send/sdtp_cli.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libsdtp.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET) $(OBJS)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(OBJS)"
