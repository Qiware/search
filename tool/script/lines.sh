#!/bin/sh

# 设置统计目录
DIR="../../src"
SRC="$DIR/exec"
SRC="$SRC $DIR/demo"
SRC="$SRC $DIR/incl"
SRC="$SRC $DIR/lib/core"
SRC="$SRC $DIR/lib/sdtp"
SRC="$SRC $DIR/lib/http"
SRC="$SRC $DIR/lib/gumbo"
SRC="$SRC $DIR/lib/redis"
SRC="$SRC $DIR/lib/invert"

# 总行数
hl=`find $SRC -name "*.h" | xargs cat | wc -l`
cl=`find $SRC -name "*.c" | xargs cat | wc -l`
total=`expr $hl + $cl`

# 空行数
hl=`find $SRC -name "*.h" | xargs grep "^$" | wc -l`
cl=`find $SRC -name "*.c" | xargs grep "^$" | wc -l`
empty=`expr $hl + $cl`

# 注释数
hl=`find $SRC -name "*.h" | xargs grep \
   -e "^[[:space:]]\{0,\}/\*" \
   -e "^[[:space:]]\{0,\}\*" \
   -e "^[[:space:]]\{0,\}\*\*" \
   -e "^[[:space:]]\{0,\}\*/" \
   -e "^[[:space:]]\{0,\}//" | wc -l`
cl=`find $SRC -name "*.c" | xargs grep \
   -e "^[[:space:]]\{0,\}/\*" \
   -e "^[[:space:]]\{0,\}\*" \
   -e "^[[:space:]]\{0,\}\*\*" \
   -e "^[[:space:]]\{0,\}\*/" \
   -e "^[[:space:]]\{0,\}//" | wc -l`
note=`expr $hl + $cl`

# 实行数
real=`expr $total - $empty - $note`

echo "总行数  空行数  注释数  实行数"
echo "$total\t$empty\t$note\t$real"
