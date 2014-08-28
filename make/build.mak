include $(PROJ)/make/switch.mak
include $(PROJ)/make/options.mak

CC = gcc
CFLAGS = -Wall -g
CFLAGS += $(patsubst %, -D%, $(OPTIONS))
LFLAGS = -Wall -fPIC -shared 
