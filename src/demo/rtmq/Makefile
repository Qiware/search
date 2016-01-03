###############################################################################
## Coypright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
		  -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/incl/rtmq \
		  -I$(PROJ)/src/incl/qwmq
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread -lcore -lrtmq

SRC_LIST = rtmq_send.c
SRC_LIST2 = rtmq_recv.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
OBJS2 = $(subst .c,.o, $(SRC_LIST2)) 
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = rtmq_send
TARGET2 = rtmq_recv

.PHONY: all clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN)
	@rm -fr $(OBJS)
	@echo "$@ is OK!"
$(TARGET2): $(OBJS2)
	@$(CC) $(CFLAGS) -o $@ $(OBJS2) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN)
	@rm -fr $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"
$(OBJS2): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"


clean:
	@rm -fr *.o $(PROJ_BIN)/$(TARGET) $(PROJ_BIN)/$(TARGET2)
	@echo "rm -fr *.o $(PROJ_BIN)/$(TARGET) $(PROJ)/$(TARGET2)"
