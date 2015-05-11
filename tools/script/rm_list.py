#!/usr/bin/env python

import os
import sys
import time

# Print file list
def print_list(fobj, path):
    idx = 0
    fpath = ""
    try:
        ls = ((f) for f in os.listdir(path))
        for item in ls:
            fpath = path + "/" + item
            if (os.path.isdir(fpath)):
                print_list(fobj, fpath)
                print "[DEBUG]: Switch directory! fpath:%s" % (fpath)
                continue
            print "%s" % (fpath)
            fobj.write(fpath+"\n")
    except BaseException, e:
        print e 

# Delete file list
def rm_list(out_path):
    ls = ((f) for f in open(out_path))
    idx = 0
    for item in ls:
        if (os.path.isdir(item)):
            continue
        try:
            os.remove(item.strip())
        except BaseException, e:
            #print "[ERROR]: %s %s" % (item, str(e))
            continue
        idx += 1
        #print "[%d] %s" % (idx, item.strip())
    #os.remove(out_path)

if __name__ == '__main__':
    out_path = "list.rm"
    fobj = open(out_path, "w")
    print_list(fobj, sys.argv[1])
    fobj.close()

    rm_list(out_path)
