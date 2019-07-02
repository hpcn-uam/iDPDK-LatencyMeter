#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#DPDK Directory
export RTE_SDK=$DIR/../dpdk

#DPDK (default) TARGET
if [ -z "$RTE_TARGET" ]
then
        export RTE_TARGET=x86_64-native-linuxapp-gcc
fi

cd $DIR/..
git submodule update --init
cd src
make -j5 && \
\
#        sudo build/app/hpcn_latency -c FFFF -n 4 --socket-mem "8000,8000" --proc-type primary --file-prefix "rte_a" -- --rx "(0,0,7)" --tx "(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15)" \
#        sudo build/app/hpcn_latency -c 0xFFC00FFF01 -n 4 -- --rx "(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,30),(0,11,31),(0,12,32),(0,13,33),(0,14,34),(0,15,35),(0,16,36),(0,17,37),(0,18,38),(0,19,39)" --tx ""\
#         gdb --args build/app/hpcn_latency -c 0xFFC00FFF01 -n 4 -- --rx "(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,30),(0,11,31),(0,12,32),(0,13,33),(0,14,34),(0,15,35)" --tx ""\
        sudo build/app/hpcn_latency -c 0xFFC00FFF00 -n 4 -- --rx "(0,0,9)" --tx "(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,30),(0,11,31),(0,12,32),(0,13,33),(0,14,34),(0,15,35),(0,16,36),(0,17,37),(0,18,38),(0,19,39)" \
                --rsz "1024, 2048" \
                --bsz "144, 144" \
		--trainSleep 0 $@
