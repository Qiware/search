###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 搜索服务模块
## 作  者: # Qifeng.zou # 2014.11.15 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I$(PROJ)/src/incl \
			-I$(PROJ)/src/incl/srch \
			-I$(PROJ)/src/incl/sdtp
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lxd_srch -lxd_core -lsdtp

SRC_LIST = search.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = search

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "CC $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "CC $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET)"
