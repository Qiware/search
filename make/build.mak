include $(PROJ)/make/switch.mak
include $(PROJ)/make/options.mak

CC = gcc
CFLAGS = -Wall -g -fPIC -s \
			-Wshadow \
			-Wcast-qual \
			-Winline \
			-Waggregate-return \
			-Wunreachable-code \
			-Wcast-align \
			-Wredundant-decls
CFLAGS += $(patsubst %, -D%, $(OPTIONS))
LFLAGS = -Wall -g -fPIC -shared  -s
