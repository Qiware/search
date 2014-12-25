#!/bin/sh

hl=`find ../../ -name "*.h" | xargs grep "^$" | wc -l`
cl=`find ../../ -name "*.c" | xargs grep "^$" | wc -l`
total=`expr $hl + $cl`
echo empty lines: $hl + $cl = $total

hl=`find ../../ -name "*.h" | xargs grep -v "^$" | wc -l`
cl=`find ../../ -name "*.c" | xargs grep -v "^$" | wc -l`
total=`expr $hl + $cl`
echo non-empty lines: $hl + $cl = $total

hl=`find ../../ -name "*.h" | xargs cat | wc -l`
cl=`find ../../ -name "*.c" | xargs cat | wc -l`
total=`expr $hl + $cl`
echo total lines: $hl + $cl = $total
