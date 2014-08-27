include switch.mak
include options.mak

CC = gcc
CFLAGS = -Wall -g
CFLAGS += $(patsubst %, -D%, $(OPTIONS))
LFLAGS = -Wall -fPIC -shared 
