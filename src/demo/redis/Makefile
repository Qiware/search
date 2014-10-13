###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib -L/usr/local/lib
LIBS = -lpthread -lxdocore -lxdounix -lhiredis

SRC_LIST = redis.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = redis_test

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@rm -fr $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(TARGET)
