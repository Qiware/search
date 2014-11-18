###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.11.15 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I./incl \
			-I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread
LIBS2 = -lxd_srch -lxd_core

SRC_LIST = srch_comm.c \
			srch_conf.c \
			srch_listen.c \
			srch_recver.c \
			srch_worker.c
SRC_LIST2 = search.c

OBJS = $(subst .c,.o, $(SRC_LIST))
OBJS2 = $(subst .c,.o, $(SRC_LIST2))

TARGET = libxd_srch.so
TARGET2 = search

.PHONY: all clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(INCLUDE) $(LIBS_PATH) $(LIBS) $(LIBS2)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

$(OBJS2): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(OBJS2) \
			$(PROJ_LIB)/$(TARGET) \
			$(PROJ_BIN)/$(TARGET2)
