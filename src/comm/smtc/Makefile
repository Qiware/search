###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 通用消息传输协议接收模块的编译
## 作  者: # Qifeng.zou # 2014.12.29 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I./incl \
		  -I. -I$(PROJ)/src/incl

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_core

SRC_LIST = recv/smtc.c \
			recv/smtc_lsn.c \
			recv/smtc_rsvr.c \
			recv/smtc_worker.c \
			send/smtc_ssvr.c \
			cli/smtc_cli.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libsmtc.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET)"
