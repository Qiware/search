###############################################################################
## Copyright(C) 2014-2024 Xundao technology Co., Ltd
##
## 功    能: 遍历编译目录，并执行指定的操作
##			1. 编译操作
##			2. 删除操作
##			3. 重新编译 
## 注意事项: 
##			1. 当需要增加编译目录时，请将目录加入变量DIR中,
##				不用修改该文件其他数据
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################
# 根目录
export PROJ = ${PWD}
export PROJ_BIN = ${PROJ}/bin
export PROJ_LIB = ${PROJ}/lib
export PROJ_LOG = ${PROJ}/log
export GCC_LOG = ${PROJ_LOG}/gcc.log

# 编译目录(注：编译按顺序执行　注意库之间的依赖关系)
#DIR=`find ./src -type d`
DIR := "src/core"
DIR += "src/comm/http"
DIR += "src/comm/gumbo"
DIR += "src/comm/redis"

DIR += "src/exec/crawler/"
DIR += "src/exec/logsvr"
DIR += "src/exec/search"

# 测试目录
DEMO_DIR = "src/demo"
#DIR += "${DEMO_DIR}/log"
#DIR += "${DEMO_DIR}/tcp"
#DIR += "${DEMO_DIR}/http"
#DIR += "${DEMO_DIR}/html"
#DIR += "${DEMO_DIR}/list"
DIR += "${DEMO_DIR}/redis"
DIR += "${DEMO_DIR}/gumbo"
export DIR

.PHONY: all clean rebuild

# 1. 编译操作
all:
	@mkdir -p ${PROJ_LIB};
	@mkdir -p ${PROJ_BIN};
	@mkdir -p ${PROJ_LOG};
	@for SUBDIR in ${DIR}; \
	do \
		if [ -e $${SUBDIR}/Makefile ]; then \
			echo cd $${SUBDIR}; \
			cd $${SUBDIR}; \
			make 2>&1 | tee -a ${GCC_LOG}; \
			cd ${PROJ}; \
		fi \
	done

# 2. 清除操作
clean:
	@for SUBDIR in ${DIR}; \
	do \
		if [ -e $${SUBDIR}/Makefile ]; then \
			echo cd $${SUBDIR}; \
			cd $${SUBDIR}; \
			make clean; \
			cd ${PROJ}; \
		fi \
	done

# 3. 重新编译 
rebuild: clean all
