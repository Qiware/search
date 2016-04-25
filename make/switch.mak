###############################################################################
## Copyright(C) 2014-2024 Qiware technology Co., Ltd
##
## 功    能: 宏开关模块
##        通过此模块中各值的设置，可以控制项目源码的编译.
## 注意事项: 
##        请勿随意修改此文件
## 作    者: # Qifeng.zou # 2014.08.28 #
###############################################################################

CONFIG_DEFAULT_SUPPORT = __ON__		# 默认功能开关
CONFIG_DEBUG_SUPPORT = __ON__		# 调试开关
CONFIG_MEMALIGN_SUPPORT = POSIX_MEMALIGN	# MEMALIGN # 内存对齐方式
CONFIG_MEMLEAK_CHECK = __OFF__		# 内存泄露测试
CONFIG_RTTP_SUPPORT = __ON__ 		# 开启实时传输功能
CONFIG_JEMALLOC_SUPPORT = __OFF__ 	# 使用Jemalloc内存池
