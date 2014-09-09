###############################################################################
## Copyright(C) 2014-2024 Xundao technology Co., Ltd
##
## 功    能: 此模块用于添加功能宏
##         	 通过switch.mak中开关的设置，在此处增加需要加载的宏
## 注意事项: 添加相关编译宏时, 请使用统一的风格.
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################
# 调试相关宏
ifeq (__ON__, $(strip $(CONFIG_DEBUG_SUPPORT)))
	OPTIONS += __XDO_DEBUG__
endif

# 日志相关宏
ifeq (__ON__, $(strip $(CONFIG_ALOG_SUPPORT)))
	OPTIONS += __ASYNC_LOG__		# 异步日志功能
endif

# XML相关宏
ifeq (__ON__, $(strip $(CONFIG_XML_SUPPORT)))
	OPTIONS += __XML_MEM_POOL__		# XML的处理过程的内存从内存池申请
	# 功能: 节点只有孩子节点或只有数值(Either Child Or Value)
	#OPTIONS += __XML_EITHER_CHILD_OR_VALUE__
endif

# HTML相关宏
ifeq (__ON__, $(strip $(CONFIG_HTML_SUPPORT)))
	OPTIONS += __HTML_AUTO_RESTORE__	# HTML自动修复功能
	OPTIONS += __HTML_DEL_BR__			# HTML处理过程中，自动删除<br />
endif

# 内存对齐
ifeq (POSIX_MEMALIGN, $(strip $(CONFIG_MEMALIGN_SUPPORT)))
	OPTIONS += HAVE_POSIX_MEMALIGN	# POSIX内存对齐方式
else ifeq (MEMALIGN, $(strip $(CONFIG_MEMALIGN_SUPPORT)))
	OPTIONS += HAVE_MEMALIGN		# 内存对齐方式
endif
