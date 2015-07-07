###############################################################################
## Copyright(C) 2014-2024 Xundao technology Co., Ltd
##
## 功    能: 遍历编译目录，并执行指定的操作
##			1. 编译操作
##			2. 删除操作
##			3. 重新编译 
## 注意事项: 
##			1. 当需要增加编译目录时, 请将目录加入变量DIR中, 不用修改该文件其他数据!
## 			2. 如果只想编译指定目录的代码, 则可执行命令:
## 				Make DIR=指定目录 如: Make SRC=src/lib/core
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################
# 根目录
export PROJ = ${PWD}
export PROJ_BIN = ${PROJ}/bin
export PROJ_LIB = ${PROJ}/lib
export PROJ_LOG = ${PROJ}/log
export PROJ_CONF = ${PROJ}/conf
export GCC_LOG = ${PROJ_LOG}/gcc.log

# 编译目录(注：编译按顺序执行　注意库之间的依赖关系)
LIB_DIR = "src/lib"
SRC = "$(LIB_DIR)/core"
SRC += "$(LIB_DIR)/conf"
SRC += "$(LIB_DIR)/rtmq"
SRC += "$(LIB_DIR)/sdtp"
SRC += "$(LIB_DIR)/gumbo"
SRC += "$(LIB_DIR)/redis"

SRC += "$(LIB_DIR)/agent"
SRC += "$(LIB_DIR)/invert"

EXEC_DIR = "src/exec"
SRC += "$(EXEC_DIR)/frwder"
SRC += "$(EXEC_DIR)/mmexec"
SRC += "$(EXEC_DIR)/crawler"
SRC += "$(EXEC_DIR)/filter"
SRC += "$(EXEC_DIR)/logsvr"
SRC += "$(EXEC_DIR)/listend"
SRC += "$(EXEC_DIR)/monitor"
SRC += "$(EXEC_DIR)/invertd"

# 创建目录结构
define MkDir
	mkdir -p ${PROJ_LIB};
	mkdir -p ${PROJ_BIN};
	mkdir -p ${PROJ_LOG};
	rm -fr ${GCC_LOG};
endef

.PHONY: all clean rebuild

# 1. 编译操作
all:
	${MkDir}
	@for ITEM in ${SRC}; \
	do \
		if [ -e $${ITEM}/Makefile ]; then \
			cd $${ITEM}; \
			make 2>&1 | tee -a ${GCC_LOG}; \
			cd ${PROJ}; \
		fi \
	done

# 2. 清除操作
clean:
	@for ITEM in ${SRC}; \
	do \
		if [ -e $${ITEM}/Makefile ]; then \
			cd $${ITEM}; \
			make clean; \
			cd ${PROJ}; \
		fi \
	done

# 3. 重新编译 
rebuild: clean all
