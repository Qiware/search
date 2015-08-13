#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os
import re
import sys

# 全局变量
g_note_lines = 0    # 注释行数
g_real_lines = 0    # 实行行数
g_empty_lines = 0   # 空行行数
g_total_lines = 0   # 所有行数

# 设置统计目录
def set_dir_list():
    dir_list = []
    path = "../../src"

    dir_list.append(path + "/exec")
    dir_list.append(path + "/demo")
    dir_list.append(path + "/incl")
    dir_list.append(path + "/lib")

    return dir_list

# 总行数
def get_total_line(path):
    global g_note_lines     # 注释行数
    global g_real_lines     # 实行行数
    global g_empty_lines    # 空行行数
    global g_total_lines    # 所有行数

    ldir = os.listdir(path)

    for item in ldir:
        abs_path = path+"/"+item
        if os.path.isdir(abs_path):
            get_total_line(abs_path)
            continue
        ext = os.path.splitext(item)[1] # splitext()返回元组(filename, extension)
        if (".c" == ext) or (".h" == ext):
            for line in open(abs_path):
                line = line.strip()
                g_total_lines += 1
                # 查找注释
                m = re.match("(\s{0,}/\*)|(\s{0,}\*)|(\s{0,}\*\*)|(\s{0,}\*/)|(\s{0,}//)", line)
                if m is not None:
                    g_note_lines += 1
                    continue;
                # 查找空行
                m = re.match("\s{0,}$", line)
                if m is not None:
                    g_empty_lines += 1
                    continue
                # 其他属实行
                g_real_lines += 1

if __name__ == "__main__":
    try:
        dir_list = set_dir_list()
        for path in dir_list:
            get_total_line(path)

        print "总行数   空行数   注释数   实行数"
        print "%-8d %-8d %-8d %-8d" % (g_total_lines, g_empty_lines, g_note_lines, g_real_lines)
    except BaseException, e:
        print e
