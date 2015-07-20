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
AR = ar

# -Wall: 显示所有警告信息
# -Werror: 所有警告信息都按错误处理
CFLAGS = -Wall -gdwarf-2 -g3 -fPIC -O0 -fstack-protector-all -fbounds-check -rdynamic \
			-Werror \
			-Wshadow \
			-Wcast-qual \
			-Wcast-align \
			-Wsign-compare \
			-Wredundant-decls \
			-finline-functions \
			-Wunreachable-code \
			-Waggregate-return \
			-Wno-unused-result \
			-Wbad-function-cast \
			-Wno-unused-function

CFLAGS += $(patsubst %, -D%, $(OPTIONS))
LFLAGS = -shared -Wall -g -fPIC -fstack-protector-all -fbounds-check -rdynamic
AFLAGS = -c -r
