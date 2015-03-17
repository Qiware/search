###############################################################################
## Coypright(C) 2014-2024 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 爬虫模块
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
			crwl_man.c \
			crwl_conf.c \
			crwl_sched.c \
			crwl_worker.c
SRC_LIST2 = crawler.c

OBJS = $(subst .c,.o, $(SRC_LIST))
OBJS2 = $(subst .c,.o, $(SRC_LIST2))

TARGET = libxd_crwl.so
TARGET2 = crawler

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

$(OBJS): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

$(OBJS2): %.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	@echo "$(CC) $(PWD)/$<"

clean:
	@rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_BIN)/$(TARGET2)
	@echo "rm -fr *.o $(PROJ_LIB)/$(TARGET) $(PROJ_BIN)/$(TARGET2)"
