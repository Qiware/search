################################################################################
## Coypright(C) 2014-2024 Qiware technology Co., Ltd
##
## 文件名: oprofile.sh
## 版本号: 1.0
## 描  述: 
## 作  者: # Qifeng.zou # Wed 29 Jul 2015 12:02:39 PM CST #
################################################################################
#!/bin/bash

opcontrol --shutdown
opcontrol --deinit
echo 0 > /proc/sys/kernel/nmi_watchdog

opcontrol --init
opcontrol --no-vmlinux
opcontrol --reset
opcontrol --start
