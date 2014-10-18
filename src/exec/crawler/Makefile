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
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/gumbo
LIBS_PATH = -L$(PROJ)/lib -L/usr/lib/hiredis
LIBS = -lpthread -lhiredis -lxd_core -lxd_http -lxd_gumbo_ex

SRC_LIST = crwl_main.c \
			crwl_task.c \
			crwl_sched.c \
			crwl_worker.c
SRC_LIST2 = parser.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
OBJS2 = $(subst .c,.o, $(SRC_LIST2)) 

TARGET = crawler
TARGET2 = parser

.PHONY: all clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"
	@rm -fr $(OBJS)

$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"
	@rm -fr $(OBJS2)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

$(OBJS2): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	@rm -fr $(OBJS) $(OBJS2) $(PROJ_BIN)/$(TARGET) $(PROJ_BIN)/$(TARGET2)
