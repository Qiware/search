#!/bin/sh

# 设置统计目录
DIR="../../src"
SRC="$DIR/exec"
SRC="$SRC $DIR/demo"
SRC="$SRC $DIR/incl"
SRC="$SRC $DIR/lib/"

# 总行数
total=`find $SRC -name "*.h" -o -name "*.c" | xargs cat | wc -l`

# 空行数
empty=`find $SRC -name "*.h" -o -name "*.c" | xargs grep "^[[:space:]]\{0,\}$" | wc -l`

# 注释数
note=`find $SRC -name "*.h" -o -name "*.c" | xargs grep \
     -e "^[[:space:]]\{0,\}/\*" \
     -e "^[[:space:]]\{0,\}\*" \
     -e "^[[:space:]]\{0,\}\*\*" \
     -e "^[[:space:]]\{0,\}\*/" \
     -e "^[[:space:]]\{0,\}//" | wc -l`

# 实行数
real=`expr $total - $empty - $note`

echo "总行数  空行数  注释数  实行数"
echo "$total\t$empty\t$note\t$real"
