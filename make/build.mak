###############################################################################
## Copyright(C) 2014-2024 Qiware technology Co., Ltd
##
## 功    能: 自动加载编译选项
## 注意事项: 请勿随意修改此文件
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################
include $(PROJ)/make/func.mak
include $(PROJ)/make/switch.mak
include $(PROJ)/make/options.mak

CC = gcc
AR = ar

# -Wall: 显示所有警告信息
# -gdwarf-2 -g3: 开启GDB调试时的额外信息 - 支持打印宏值
# -O0: 不进行优化(优化: O2 O3)
# -fstack-check: 栈的检测-准确性较差,特别是在代码执行了O2以上的优化后.
# -fstack-protector-all: 启用堆栈保护,为所有函数插入保护代码.
#   					 可以防止缓冲区溢出攻击，也可以利用此选项来检查代码是否有缓冲区溢出的错误.
# -fbounds-check: 可以执行数组边界错误,主要是检查对数组赋值越界.
# -rdynamic: 指示连接器把所有符号(而不仅仅只是程序已使用到的外部符号)都添加到动态符号表(即.dynsym表)里,
#            以便那些通过 dlopen() 或 backtrace() (这一系列函数使用.dynsym表内符号)这样的函数使用.
# -Werror: 视警告为错误;出现任何警告即放弃编译.
# -Wshadow: 一旦某个局部变量屏蔽了另一个局部变量,编译器就发出警告.
# -Wcast-qual: 一旦某个指针强制类型转换以便移除类型修饰符时,编译器就发出警告.
#              例如,如果把const char * 强制转换为普通的char *时,警告就会出现.
# -Wcast-align: 一旦某个指针类型强制转换时,导致目标所需的地址对齐(alignment)增加,编译器就发出警告.例
#               如,某些机器上 只能在2或4字节边界上访问整数,如果在这种机型上把char *强制转换成int *类
#               型, 编译器就发出警告.
# -Wsign-compare: 不同类型的数值进行比较时, 报警
# -Wredundant-decls: 如果在同一个可见域某定义多次声明,编译器就发出警告,即使这些重复声明有效并且毫无差别.
# -finline-functions: 把所有简单的函数集成进调用者.编译器探索式地决定哪些函数足够简单,值得这种集成.
#                     如果集成了所有给定函数的调用,而且函数声明为static,那么一般说来GCC有权不按汇编代码输出
#                     函数.
# -Wunreachable-code: 不可达代码, 报警
# -Waggregate-return: 如果定义或调用了返回结构或联合的函数,编译器就发出警告. (从语言角度你可以返回一个数组,然而同样会导致警告.)
# -Wno-unused-result: 存在未使用的函数返回结果, 报警
# -Wbad-function-cast: 错误的函数返回值, 报警
# -Wno-unused-function: 存在未使用的变量, 报警
CFLAGS = -Wall -gdwarf-2 -g3 -fPIC -O0 -fstack-check -fstack-protector-all -fbounds-check -rdynamic \
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
