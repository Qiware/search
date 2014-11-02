###############################################################################
## Copyright(C) 2014-2024 Xundao technology Co., Ltd
##
## 功    能: 此模块用于添加功能宏
##         	 通过switch.mak中开关的设置，在此处增加需要加载的宏
## 注意事项: 添加相关编译宏时, 请使用统一的风格.
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################


# 默认开启的功能
ifeq (__ON__, $(strip $(CONFIG_DEFAULT_SUPPORT)))
	# XML相关功能
	OPTIONS += __XML_MEM_POOL__			# XML的处理过程的内存从内存池申请
	# 功能: 节点只有孩子节点或只有数值(Either Child Or Value)
	# OPTIONS += __XML_EITHER_CHILD_OR_VALUE__
	# OPTIONS += __XML_ESC_PARSE__		# XML支持转义处理

	# HTML相关功能
	OPTIONS += __HTML_AUTO_RESTORE__	# HTML自动修复功能
	OPTIONS += __HTML_DEL_BR__			# HTML处理过程中，自动删除<br />
endif

# 事件驱动相关宏
ifeq (EPOLL, $(strip $(CONFIG_EVENT_SUPPORT)))
	OPTIONS += __EVENT_EPOLL__
else ifeq (SELECT, $(strip $(CONFIG_EVENT_SUPPORT)))
	OPTIONS += __EVENT_SELECT__
endif

# 调试相关宏
ifeq (__ON__, $(strip $(CONFIG_DEBUG_SUPPORT)))
	OPTIONS += __XD_DEBUG__
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
