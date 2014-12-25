#!/bin/sh

find ../../ -name "*.c" | xargs grep "TODO:"
find ../../ -name "*.h" | xargs grep "TODO:"
