#!/bin/sh

runs=200
for i in `seq 1 $runs`
do
    echo "RUNNING TEST $i"
    ./check_wrapper
    if test $? != 0; then
	echo "FAILED!!!"
	exit 1
    fi
done
echo "All $runs OK"