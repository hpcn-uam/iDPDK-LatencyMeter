
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
portidx=0
make -j5 && \
\
        sudo build/app/hpcn_latency -c FFFFFF -n 4 -- \
		--tx "($portidx,0,11),($portidx,1,12),($portidx,2,13),($portidx,3,14),($portidx,4,15),($portidx,5,16),($portidx,6,17),($portidx,7,18),($portidx,8,19),($portidx,9,10)" \
		--rx "($portidx,0,10)" \
                --bw --rsz "1024, 2048" \
                --bsz "144, 144" \
		--trainSleep 0 $@


#		--tx "($portidx,0,11),($portidx,1,12),($portidx,2,13),($portidx,3,14),($portidx,4,15),($portidx,5,16),($portidx,6,17),($portidx,7,18),($portidx,8,19),($portidx,9,10)" \
#		--tx "($portidx,0,11),($portidx,1,12),($portidx,2,14),($portidx,3,14)" \

