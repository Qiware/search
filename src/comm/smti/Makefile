###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 通用消息传输协议接收模块的编译
## 作  者: # Qifeng.zou # 2014.12.29 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/comm/smti/incl \
		  -I$(PROJ)/src/comm/smti/send/incl \
		  -I$(PROJ)/src/comm/smti/recv/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_core

SRC_LIST = recv/smti.c \
			recv/smti_lsn.c \
			recv/smti_rsvr.c \
			recv/smti_worker.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libsmti.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(PROJ_LIB)/$(TARGET)
