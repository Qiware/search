include $(PROJ)/make/build.mak

INCLUDE = -I. -I$(PROJ)/src/incl
LIBS_PATH = -L$(PROJ)/lib
LIBS = -lpthread

SRC_LIST = avl_tree.c \
			rb_tree.c \
			list.c \
			list2.c \
			stack.c \
			queue.c \
			common.c \
			sha256.c \
			slab.c \
			shm_slab.c \
			xml_comm.c \
			xml_print.c \
			xml_tree.c \
			thread_pool.c

OBJS = $(subst .c,.o, $(SRC_LIST)) 

TARGET = libcore.so

.PHONY: all clean

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(INCLUDE) $(LIBS_PATH) $(LIBS)
	mv $@ $(PROJ)/lib
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

clean:
	rm -fr $(OBJS)
