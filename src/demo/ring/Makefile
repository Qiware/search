###############################################################################
## Coypright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lcore

SRC_LIST = ring_demo.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = ring_demo

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@rm -fr $(OBJS)
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr $(OBJS) $(PROJ_BIN)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_BIN)/$(TARGET)"
