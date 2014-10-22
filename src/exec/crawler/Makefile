###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I./incl \
			-I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib -L/usr/lib/hiredis
LIBS = -lpthread -lhiredis -lxd_core -lxd_http -lxd_gumbo_ex

SRC_LIST = crwl_main.c \
			crwl_task.c \
			crwl_sched.c \
			crwl_worker.c \
			crwl_parser.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = crawler

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"
	@rm -fr $(OBJS)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(PROJ_BIN)/$(TARGET)
