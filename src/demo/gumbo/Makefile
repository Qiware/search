###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl -I$(PROJ)/src/gumbo
LIBS_PATH = -L$(PROJ)/lib -L/usr/local/lib
LIBS = -lgumbo -lxd_gumbo_ex -lxd_core

SRC_LIST = gumbo_get_href.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = gumbo_get_href

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "$(CC) $@"
	@rm -fr $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

clean:
	@rm -fr $(OBJS) $(TARGET)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET)"
