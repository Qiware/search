###############################################################################
## Coypright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 拷贝的测试代码
## 作  者: # Qifeng.zou # 2015.07.27 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread
LIBS_A = $(PROJ_LIB)/libcore.a

SRC_LIST = copy.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = copy

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS_A) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN) 
	@rm -fr $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr $(OBJS) $(TARGET)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET)"
