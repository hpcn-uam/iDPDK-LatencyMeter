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
        sudo build/app/hpcn_latency -c FFFF -n 4 --socket-mem "8000,8000" --proc-type primary --file-prefix "rte_a" -- --rx "(0,0,7)" --tx "(0,0,10),(0,1,13)" \
                --rsz "1024, 2048" \
                --bsz "144, 144" \
		--trainSleep 0 $@
