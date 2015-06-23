###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 配置模块的编译
## 作  者: # Qifeng.zou # 2015.06.23 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I$(PROJ)/src/incl \
		  -I$(PROJ)/src/incl/conf \
		  -I$(PROJ)/src/incl/rttp \
		  -I$(PROJ)/src/incl/agent
LIBS_PATH = -L$(PROJ)/lib

SRC_LIST = mon_conf.c \
			flt_conf.c \
			crwl_conf.c \
			frwd_conf.c \
			lsnd_conf.c \
			invtd_conf.c

OBJS = $(subst .c,.o, $(SRC_LIST))

TARGET = libxd_conf.so

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
