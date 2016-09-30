###############################################################################
## Copyright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 探针服务模块
## 作  者: # Qifeng.zou # 2014.11.15 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I./incl \
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/incl/sdtp \
			-I$(PROJ)/src/incl/rtmq \
		  	-I/usr/local/include/
INCLUDE += $(GLOBAL_INCLUDE)
LIBS_PATH = -L$(PROJ)/lib
# 注: 静态库请放在动态库之前
STATIC_LIB_LIST = librtmq.a libcore.a libutils.a
LIBS = $(call func_find_static_link_lib,$(STATIC_LIB_PATH),$(STATIC_LIB_LIST))
LIBS += -lpthread -lev -lwebsockets
LIBS += $(SHARED_LIB)

SRC_LIST = lwsd.c \
			lwsd_comm.c \
			lwsd_mesg.c \
			lwsd_conf.c \
			lwsd_search.c

OBJS = $(subst .c,.o, $(SRC_LIST))
HEADS = $(call func_get_dep_head_list, $(SRC_LIST))

TARGET = listend-ws

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN)/$@-$(VERSION)
	@rm -f $(PROJ_BIN)/$@
	@ln -s $(PROJ_BIN)/$@-$(VERSION) $(PROJ_BIN)/$@
	@echo "$@ is OK!"

$(OBJS): %.o : %.c $(HEADS)
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_BIN)/$(TARGET)
	@rm -fr *.o $(PROJ_BIN)/$(TARGET)-$(VERSION)
	@echo "rm -fr *.o $(PROJ_BIN)/$(TARGET)"
