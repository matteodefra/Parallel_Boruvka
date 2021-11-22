#!/bin/bash

export LD_PRELOAD="${PWD}/jemalloc/lib/libjemalloc.so.2"

if [ $# -ne 3 ]; then
    echo "./[program] [max_nw] [iter]"
	exit 1
fi

par=$2	    # maximum parallelism degree
iter=$3 	# number of iterations to reduce variance

echo "$iter"

num_nodes=10
num_edges=20

filename="V1000_E10000.txt"

# loop to increase the parallelism degree
# loop for multiple independent iterations to reduce variance
for((j=0; j<$iter; j++)); do
    ./$1 $par $num_nodes $num_edges $filename; 
done | grep "Total time required" | awk -v iter=$iter -v par=$par '{sum += $4} END {print sprintf("%s,%s", par, sum/iter)}'