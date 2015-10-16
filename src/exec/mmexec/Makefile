###############################################################################
## Copyright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 负责共享内存的创建
## 作  者: # Qifeng.zou # 2015.06.04 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I../agentd/incl \
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/incl/sdtp \
			-I$(PROJ)/src/incl/rtmq \
			-I$(PROJ)/src/incl/agent \
			-I$(PROJ)/src/incl/conf
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread -lcore -lconf

SRC_LIST = mmexec.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = mmexec

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_BIN)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_BIN)/$(TARGET)"
