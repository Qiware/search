CC = gcc
CFLAGS = -Wall -O2

SRCS = recv.c
OBJS = recv.o
HEADS = $(call func_get_dep_head_list, $(SRCS))
TARGET = recv

SRCS2 = send.c
OBJS2 = send.o
TARGET2 = send 

.PHONY: clean

all: $(TARGET) $(TARGET2)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2)

$(OBJS): %.o : %.c $(HEADS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS2): %.o : %.c $(HEADS)
	$(CC) $(CFLAGS) -o $@ -c $<


clean:
	rm -fr $(TARGET2) $(TARGET) $(OBJS2) $(OBJS)
