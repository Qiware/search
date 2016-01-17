include $(PROJ)/make/build.mak

CC = gcc
CFLAGS = -Wall -O2

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib -L/usr/local/lib
LIBS = -lcore

SRCS = connect.c #recv.c
OBJS = connect.o #recv.o
HEADS = $(call func_get_dep_head_list, $(SRCS))
TARGET = connect #recv

SRCS2 = send.c
OBJS2 = send.o
TARGET2 = send 

.PHONY: clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS) $(LIBS_PATH)
	mv $@ $(PROJ_BIN)
$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS) $(LIBS_PATH)
	mv $@ $(PROJ_BIN)

$(OBJS): %.o : %.c $(HEADS)
	$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDE)

$(OBJS2): %.o : %.c $(HEADS)
	$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDE)


clean:
	rm -fr $(TARGET2) $(TARGET) $(OBJS2) $(OBJS)
