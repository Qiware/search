###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/exec/search/incl \
		  -I$(PROJ)/src/exec/crawler/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread -lxd_core -lxd_srch -lxd_crwl

SRC_LIST = monitor.c \
			mon_srch.c \
			mon_crwl.c \

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = monitor

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(PROJ_BIN)/$(TARGET)
