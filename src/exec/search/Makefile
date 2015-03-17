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
			srch_agent.c \
			srch_listen.c \
			srch_worker.c
SRC_LIST2 = search.c
SRC_LIST3 = srch_client.c

OBJS = $(subst .c,.o, $(SRC_LIST))
OBJS2 = $(subst .c,.o, $(SRC_LIST2))
OBJS3 = $(subst .c,.o, $(SRC_LIST3))

TARGET = libxd_srch.so
TARGET2 = search

.PHONY: all clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(TARGET2): $(OBJS2)
	@$(CC) $(CFLAGS) -o $@ $(OBJS2) $(INCLUDE) $(LIBS_PATH) $(LIBS) $(LIBS2)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

$(OBJS2): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_BIN)/$(TARGET2)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_BIN)/$(TARGET2)"
