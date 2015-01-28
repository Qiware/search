###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 日志服务模块
## 作  者: # Qifeng.zou # 2014.09.01 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. \
			-I./incl \
			-I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib -L/usr/lib/hiredis
LIBS = -lpthread \
	   		-lhiredis \
			-lxd_redis \
			-lxd_core \
			-lxd_http \
			-lxd_gumbo_ex 
LIBS2 = -lxd_crwl

SRC_LIST = crwl_comm.c \
			crwl_conf.c \
			crwl_sched.c \
			crwl_worker.c
SRC_LIST2 = crawler.c
SRC_LIST3 = crwl_filter.c

OBJS = $(subst .c,.o, $(SRC_LIST))
OBJS2 = $(subst .c,.o, $(SRC_LIST2))
OBJS3 = $(subst .c,.o, $(SRC_LIST3))

TARGET = libxd_crwl.so
TARGET2 = crawler
TARGET3 = crwlfilter

.PHONY: all clean

all: $(TARGET) $(TARGET2) $(TARGET3)
$(TARGET): $(OBJS)
	@$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_LIB)
	@echo "$@ is OK!"

$(TARGET2): $(OBJS2)
	@$(CC) $(CFLAGS) -o $@ $(OBJS2) $(INCLUDE) $(LIBS_PATH) $(LIBS) $(LIBS2)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(TARGET3): $(OBJS3)
	@$(CC) $(CFLAGS) -o $@ $(OBJS3) $(INCLUDE) $(LIBS_PATH) $(LIBS) $(LIBS2)
	@echo "$(CC) $@"
	@mv $@ $(PROJ_BIN)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $<"

$(OBJS2): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $<"

$(OBJS3): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $<"

clean:
	@rm -fr *.o \
			$(PROJ_LIB)/$(TARGET) \
			$(PROJ_BIN)/$(TARGET2) \
			$(PROJ_BIN)/$(TARGET3)
