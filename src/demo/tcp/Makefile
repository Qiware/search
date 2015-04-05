CC = gcc
CFLAGS = -Wall -O2

SRCS = recv.c
OBJS = recv.o
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

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJS2): %.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<


clean:
	rm -fr $(TARGET2) $(TARGET) $(OBJS2) $(OBJS)
