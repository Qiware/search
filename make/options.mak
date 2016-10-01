###############################################################################
## Copyright(C) 2014-2024 Qiware technology Co., Ltd
##
## 功    能: 此模块用于添加功能宏
##         	 通过switch.mak中开关的设置，在此处增加需要加载的宏
## 注意事项: 添加相关编译宏时, 请使用统一的风格.
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################

STATIC_LIB =  # 静态链接库
SHARED_LIB =  # 动态链接库
GLOBAL_INCLUDE = # 通用头文件路径
# 静态链接库路径
STATIC_LIB_PATH = $(PROJ_LIB) \
				 /usr/lib/ \
				 /usr/local/lib \
				 $(PROJ)/../clang/lib \
				 /usr/lib/x86_64-linux-gnu/

# 默认开启的功能
ifeq (__ON__, $(strip $(CONFIG_DEFAULT_SUPPORT)))
	# XML相关功能
	# 功能: 节点只有孩子节点或只有数值(Either Child Or Value)
	# OPTIONS += __XML_EITHER_CHILD_OR_VALUE__
	# OPTIONS += __XML_ESC_PARSE__		# XML支持转义处理

	# 系统通用宏 
	OPTIONS += _GNU_SOURCE
endif

# 调试相关宏
ifeq (__ON__, $(strip $(CONFIG_DEBUG_SUPPORT)))
	OPTIONS += __XD_DEBUG__
	OPTIONS += __INVTD_DEBUG__
endif

# 内存泄露检测
ifeq (__ON__, $(strip $(CONFIG_MEMLEAK_CHECK)))
	OPTIONS += __MEM_LEAK_CHECK__
endif

# 内存对齐
ifeq (POSIX_MEMALIGN, $(strip $(CONFIG_MEMALIGN_SUPPORT)))
	OPTIONS += HAVE_POSIX_MEMALIGN	# POSIX内存对齐方式
else ifeq (MEMALIGN, $(strip $(CONFIG_MEMALIGN_SUPPORT)))
	OPTIONS += HAVE_MEMALIGN		# 内存对齐方式
endif

# 实时传输协议
ifeq (__ON__, $(strip $(CONFIG_RTTP_SUPPORT)))
	OPTIONS += __RTTP_SUPPORT__
endif

# JEMALLOC内存池
ifeq (__ON__, $(strip $(CONFIG_JEMALLOC_SUPPORT)))
	OPTIONS += __JEMALLOC_SUPPORT__
	SHARED_LIB += -ljemalloc
	GLOABL_INCLUDE += -I/usr/local/include/
endif
