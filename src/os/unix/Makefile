###############################################################################
## Coypright(C) 2013-2014 Xundao technology Co., Ltd
##
## 文件名: Makefile
## 版本号: 1.0
## 描  述: 核心模块的编译
## 作  者: # Qifeng.zou # 2014.08.28 #
###############################################################################
include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread

SRC_LIST = 	lock.c \
			xdo_unistd.c \
			xdo_socket.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libxdounix.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	mv $@ $(PROJ_LIB)
	rm -f $(OBJS)
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	rm -fr $(OBJS) $(PROJ_LIB)/$(TARGET)
