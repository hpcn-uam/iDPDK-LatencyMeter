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
        sudo build/app/hpcn_latency -c FFFF -n 4 --socket-mem "8000,8000" --proc-type auto --file-prefix "rte_b" -- --rx "(1,0,7)" --tx "(1,0,11),(1,1,12)" \
                --rsz "1024, 2048" \
                --bsz "144, 144" \
		--trainSleep 0 $@
