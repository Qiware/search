#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os
import re
import sys

# 设置统计路径
def set_dir_list():
    dir_list = []
    path = "../../src"

    dir_list.append(path + "/exec")
    dir_list.append(path + "/demo")
    dir_list.append(path + "/incl")
    dir_list.append(path + "/lib")

    return dir_list

# 统计信息
class CStatistics(object):
    # 初始化设置
    def __init__(self):
        self.note_lines = 0    # 注释行数
        self.real_lines = 0    # 实行行数
        self.empty_lines = 0   # 空行行数
        self.total_lines = 0   # 所有行数

    # 打印行数统计结果
    def print_lines(self):
        print "总行数   空行数   注释数   实行数"
        print "%-8d %-8d %-8d %-8d" % \
                (self.total_lines, self.empty_lines,\
                self.note_lines, self.real_lines)

    # 统计单个文件中的行数
    def _get_lines(self, fdir):

        flist = os.listdir(fdir)

        for fname in flist:
            fpath = fdir+"/"+fname
            if os.path.isdir(fpath):
                self._get_lines(fpath)
                continue
            ext = os.path.splitext(fname)[1] # splitext()返回元组(filename, extension)
            if (".c" == ext) or (".h" == ext):
                for line in open(fpath):
                    line = line.strip()
                    self.total_lines += 1
                    # 查找注释
                    m = re.match("(^\s{0,}/\*)|(^\s{0,}\*)|(^\s{0,}\*\*)|(^\s{0,}\*/)|(^\s{0,}//)", line)
                    if m is not None:
                        self.note_lines += 1
                        continue;
                    # 查找空行
                    m = re.match("^\s{0,}$", line)
                    if m is not None:
                        self.empty_lines += 1
                        continue
                    # 其他属实行
                    self.real_lines += 1
    # 统计目录列表所有文件行数
    def get_lines(self, dir_list):
        for path in dir_list:
            self._get_lines(path)

# 主函数
if __name__ == "__main__":
    stat = CStatistics()
    dir_list = set_dir_list()

    try:
        stat.get_lines(dir_list)
        stat.print_lines()
    except BaseException, e:
        print e
