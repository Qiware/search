###############################################################################
## Copyright(C) 2015-2024 Qiware technology Co., Ltd
##
## 功    能: 函数的定义
## 注意事项: 请勿随意修改此文件
## 作    者: # Qifeng.zou # 2015.07.24 01:08:09 #
###############################################################################

# 创建目录结构
define func_mkdir
	mkdir -p ${PROJ_LIB};
	mkdir -p ${PROJ_BIN};
	mkdir -p ${PROJ_LOG};
	rm -fr ${GCC_LOG};
endef

# 获取CPU核数
define func_cpu_cores
	$(shell cat /proc/cpuinfo | grep "cpu cores" | awk -F: 'BEGIN {cpu_cores=0} {cpu_cores+=$$2} END{print cpu_cores}')
endef

# 获取源文件的所依赖的头文件列表
# 参数1: 单个源文件(如: list.c)
# 注意: tr -d [\\]含义是删除反斜杠字符'\' - 请参考tr用法
define _func_get_dep_head_list
	$(shell $(CC) -MM $(INCLUDE) $(1) \
			| tr -d [\\] \
			| awk '{ for(idx=1; idx<=NF; ++idx) { if ($$idx ~ /\.h/) {print $$idx}} }')
endef

# 获取源文件列表的所依赖的头文件列表
# 参数1: 源文件列表(ex: avl_tree.c list.c queue.c, etc.)
define func_get_dep_head_list
	$(sort $(foreach item, $(1), $(call _func_get_dep_head_list, $(item))))
endef

# 查找存在制定静态链接库的路径
# 参数1: 路径列表
# 参数2: 单个链接库
# 注 意: 找到第一个找到的路径后, 结束本轮的查找出路.
define _func_find_static_link_lib
	$(shell \
		for path in $(1); \
		do \
			if [ -f $$path/$(2) ]; then \
				echo $$path/$(2); \
				break; \
			fi \
		done \
	)
endef

# 查找静态链接库的路径
# 参数1: 路径列表
# 参数2: 链接库列表
# 注意: 
# 	1. 本函数自动对路径列表和链接库进行去重处理
#   2. 给函数_func_find_static_link_lib()传参时, 不要在逗号后面有"空格"!!! 负责参数中带空格.
define func_find_static_link_lib
	$(foreach lib, $(2), $(call _func_find_static_link_lib,$(sort $(1)),$(lib)))
endef
