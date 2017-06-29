#!/usr/bin/sh

cat $0
echo "-------------------------------"
date
uname -a
git describe
echo "-------------------------------"

export LD_PRELOAD=/home/ahori/work/PIP/PIP/preload/pip_preload.so

for NTASKS in 1 2 4 8 16 32 64 128
do
    for PROG in spawn-pip spawn-forkexec spawn-vforkexec spawn-posix spawn-thread
    do
	if [ $PROG == spawn-pip ]
	then
	    for PIPMODE in process:preload process:pipclone thread
	    do
		export PIP_MODE=$PIPMODE
		for ITER in 1 2 3 4 5 6 7 8 9 10 11 12
		do
		    numactl -C 1 ./$PROG $NTASKS $NTASKS
		    #./$PROG $NTASKS $NTASKS
		done
		unset PIP_MODE
		echo
	    done
	else
	    for ITER in 1 2 3 4 5 6 7 8 9 10 11 12
	    do
		numactl -C 1 ./$PROG $NTASKS $NTASKS
		#./$PROG $NTASKS $NTASKS
	    done
	    echo
	fi
    done
    echo
done
