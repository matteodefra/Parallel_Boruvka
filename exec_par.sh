#!/bin/bash

export LD_PRELOAD="${PWD}/jemalloc/lib/libjemalloc.so.2"

if [ $# -ne 4 ]; then
    echo "./[program] [max_nw] [iter] [filename]"
	exit 1
fi

par=$2	    # maximum parallelism degree
iter=$3 	# number of iterations to reduce variance

num_nodes=10
num_edges=20

filename=$4

# loop to increase the parallelism degree
# loop for multiple independent iterations to reduce variance
./build/$1 $par $num_nodes $num_edges $filename $iter >> results/execution_"$1".txt;
# grep "Total time required" | awk -v iter=$iter -v par=$par '{sum += $4} END {print sprintf("%s,%s", par, sum/iter)}'