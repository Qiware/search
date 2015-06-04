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
LIBS = -lxd_core

SRC_LIST = recv/drcv_recv.c \
			recv/drcv_comm.c \
			recv/drcv_lsn.c \
			recv/drcv_rsvr.c \
			recv/drcv_worker.c \
			recv/drcv_cli.c \
			send/dsnd_send.c \
			send/dsnd_pool.c \
			send/dsnd_ssvr.c \
			send/dsnd_worker.c \
			send/dsnd_cli.c

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
