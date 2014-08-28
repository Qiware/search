# 根目录
export PROJ=$PWD

# 编译目录(注：编译按顺序执行　注意库之间的依赖关系)
DIR="src/os/unix
        src/core
        src/crawler"

for SUBDIR in $DIR      # 遍历源码目录
do
    if [ -e $SUBDIR/Makefile ]; then
        echo cd $SUBDIR
        cd $SUBDIR
        make            # 执行编译操作
        cd $PROJ
    fi
done
