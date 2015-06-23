###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 探针服务模块
## 作  者: # Qifeng.zou # 2014.11.15 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I./incl \
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/incl/conf \
			-I$(PROJ)/src/incl/sdtp \
			-I$(PROJ)/src/incl/rttp \
			-I$(PROJ)/src/incl/agent
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_agent -lxd_core -lsdtp -lrttp -lxd_conf

SRC_LIST = listend.c \
			lsnd_comm.c \
			lsnd_dist.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = listend

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
