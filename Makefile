
# 根目录
export PROJ = ${PWD}

# 编译目录(注：编译按顺序执行　注意库之间的依赖关系)
DIR := "src/core"
DIR += "src/os/unix"
DIR += "src/src/crawler"
export DIR

.PHONY: all clean rebuild

# 编译操作
all:
	@for SUBDIR in ${DIR}; \
	do \
		if [ -e $${SUBDIR}/Makefile ]; then \
			echo cd $${SUBDIR}; \
			cd $${SUBDIR}; \
			make; \
			cd ${PROJ}; \
		fi \
	done

# 清除操作
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

# 重新编译 
rebuild: clean all
