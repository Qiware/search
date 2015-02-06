#!/bin/sh

# 总行数
hl=`find ../../ -name "*.h" | xargs cat | wc -l`
cl=`find ../../ -name "*.c" | xargs cat | wc -l`
total=`expr $hl + $cl`

# 空行数
hl=`find ../../ -name "*.h" | xargs grep "^$" | wc -l`
cl=`find ../../ -name "*.c" | xargs grep "^$" | wc -l`
empty=`expr $hl + $cl`

# 注释数
hl=`find ../../ -name "*.h" | xargs grep -e "\*\*" -e "/\*" -e "\*\/" | wc -l`
hl=`find ../../ -name "*.c" | xargs grep -e "\*\*" -e "/\*" -e "\*\/" | wc -l`
note=`expr $hl + $cl`

# 实行数
real=`expr $total - $empty - $note`

echo "总行数  空行数  注释数  实行数"
echo "$total\t$empty\t$note\t$real"
