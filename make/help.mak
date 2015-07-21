--------------------------------------------------------------------------------
                                  编译说明
--------------------------------------------------------------------------------

[编译]
    make                 -- Build project
    make all             -- Build project. Same with 'make'
    make rebuild         -- Rebuild project
                            Same with execute 'make clean ' then 'make all'

[清理]
    make clean           -- Clean *.o *.so *.a, etc.

[其他]
    make help            -- Display help information.

--------------------------------------------------------------------------------

[参数]
    DIR={dir}
             控制Makefile值编译某些指定的目录, 有优先级区别.
             如'make DIR=src/exec/monitor'只编译monitor工具. 想编译多个目录请
             执行类似'make DIR=src/lib/core DIR+=src/lib/rtmq'操作.

-----------------------------------------------------------------------------
