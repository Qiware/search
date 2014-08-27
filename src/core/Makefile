include ../../make/build.mak

INCLUDE = ../../incl/
LIBS_PATH = -L../../lib
LIBS = -lpthread

SRC_LIST = avl_tree.c \
		rb_tree.c 
		list.c \
		list2.c \
		stack.c \
		queue.c \
		xdt_comm.c \
		md5.c \
		sha256.c \
		slab.c \
		xml_comm.c \
		xml_tree.c \
		thread_pool.c

OBJS := $(subst .c, .o, $(SRC_LIST)) 

TARGET = libcomm.so

all: $(TARGET)
	$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) $(INCLUDE) $(LIBS_PATH) $(LIBS) -o $@ $(OBJS)
	mv ../../lib
	@echo "$@ is OK!"

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -fr $(OBJS)
