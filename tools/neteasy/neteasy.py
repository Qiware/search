#!/usr/bin/env python2
# -*- coding: utf-8 -*-
import os
import re
import sys

if "__main__" == __name__:
    num = len(sys.argv)
    if 1 == num:
        print("Get file name failed!")
        exit(-1)

    fname = sys.argv[1]

    fp = open(fname, "r")
    for line in fp.readlines():
        #print("line:", line)
        m = re.search(r"[A-Z0-9]{16}", line)
        if m:
            print(m.group(0))
