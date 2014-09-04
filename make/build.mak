###############################################################################
## Copyright(C) 2014-2024 Xundao technology Co., Ltd
##
## 功    能: 自动加载编译选项
## 注意事项: 
##		请勿随意修改此文件
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################
include $(PROJ)/make/switch.mak
include $(PROJ)/make/options.mak

CC = gcc
CFLAGS = -Wall -g -fPIC -O0\
			-Werror \
			-Wshadow \
			-Winline \
			-Wcast-qual \
			-Wunreachable-code \
			-Waggregate-return \
			-Wcast-align  \
			-Wredundant-decls

CFLAGS += $(patsubst %, -D%, $(OPTIONS))
LFLAGS = -Wall -g -fPIC -shared  -s
