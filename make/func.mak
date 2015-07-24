###############################################################################
## Copyright(C) 2015-2024 Xundao technology Co., Ltd
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
