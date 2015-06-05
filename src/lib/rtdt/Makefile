###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 实时数据传输模块的编译
## 作  者: # Qifeng.zou # 2015.06.05 23:14:02 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
		  -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/incl/rtdt

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_core

SRC_LIST = rtrd_recv.c \
			rtrd_comm.c \
			rtrd_lsn.c \
			rtrd_rsvr.c \
			rtrd_worker.c \
			rtrd_cli.c \
			rtsd_send.c \
			rtsd_ssvr.c \
			rtsd_worker.c \
			rtsd_cli.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = librtdt.so

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
