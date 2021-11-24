#!/bin/bash

export LD_PRELOAD="${PWD}/jemalloc/lib/libjemalloc.so.2"

if [ $# -ne 3 ]; then
    echo "./[program] [iter] [filename]"
	exit 1
fi

iter=$2	# number of iterations to reduce variance

num_nodes=10
num_edges=20

filename=$3

# loop to increase the parallelism degree
# loop for multiple independent iterations to reduce variance
./$1 $num_nodes $num_edges $filename $iter >> execution_seq.txt
# grep "Total time required" | awk -v iter=$iter -v par=$par '{sum += $4} END {print sprintf("%s,%s", par, sum/iter)}'