###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 倒排服务模块
## 作  者: # Qifeng.zou # 2015.03.06 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I./incl \
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/incl/sdtp \
			-I$(PROJ)/src/incl/rttp \
			-I$(PROJ)/src/incl/agent

LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread -lxd_core -lsdtp -lrttp

SRC_LIST = frwder.c \
			frwd_comm.c \
			frwd_conf.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = frwder

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_BIN)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_BIN)/$(TARGET)"
