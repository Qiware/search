###############################################################################
## Coypright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: B树的测试代码
## 作  者: # Qifeng.zou # 2015.04.21 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lcore

SRC_LIST = btree_demo.c
SRC_LIST2 = shm_btree_demo.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
OBJS2 = $(subst .c,.o, $(SRC_LIST2)) 
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = btree_demo 
TARGET2 = shm_btree_demo

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
	@rm -fr $(OBJS2)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"
$(OBJS2): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr $(OBJS) $(TARGET) $(TARGET2)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_LIB)/$(TARGET2)"
