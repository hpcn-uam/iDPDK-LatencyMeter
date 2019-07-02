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
#git submodule update --init
cd src

CONFIGS='"(0,0,10)" '\
'"(0,0,10),(0,1,10)" '\
'"(0,0,10),(0,1,11)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,30),(0,5,31),(0,6,32),(0,7,33)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,30),(0,9,31),(0,10,32),(0,11,33),(0,12,34),(0,13,35),(0,14,36),(0,15,37)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,10),(0,9,11),(0,10,12),(0,11,13),(0,12,14),(0,13,15),(0,14,16),(0,15,17)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,30),(0,9,31),(0,10,32),(0,11,33),(0,12,34),(0,13,35),(0,14,36),(0,15,37),(0,16,38),(0,17,39)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,0),(0,11,1),(0,12,2),(0,13,3),(0,14,4),(0,15,5),(0,16,6),(0,17,7),(0,18,8),(0,19,9)" '\
'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,30),(0,11,31),(0,12,32),(0,13,33),(0,14,34),(0,15,35),(0,16,36),(0,17,37),(0,18,38),(0,19,39)"'
#'"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,18),(0,9,19),(0,10,10),(0,11,11),(0,12,12),(0,13,13),(0,14,14),(0,15,15),(0,16,16),(0,17,17),(0,18,18),(0,19,19)"'

#CONFIGS='"(0,0,10),(0,1,11),(0,2,12),(0,3,13),(0,4,14),(0,5,15),(0,6,16),(0,7,17),(0,8,30),(0,9,31),(0,10,32),(0,11,33),(0,12,34),(0,13,35),(0,14,36),(0,15,37)"'

TESTLENGTH=300

make && \
for conf in $CONFIGS
do
	echo STARTING TEST $conf ...
#	ssh hpcn@onuris.ii.uam.es /home/hpcn/Desktop/Prueba_VCU108/gitlab/virtex7-dma-core/HOST/bin/rwBar w 2 0x3000 1
	sleep $TESTLENGTH && killall build/app/hpcn_latency &
        stdbuf -i0 -o0 -e0 build/app/hpcn_latency -c 0xffffffffff -n 2 -- --rx $conf --tx "(0,0,0)" --rsz "1024, 1024" --bsz "144, 144" --bwp | grep drop
	echo JUST STOPED TEST $conf ...
#	ssh hpcn@onuris.ii.uam.es /home/hpcn/Desktop/Prueba_VCU108/gitlab/virtex7-dma-core/HOST/bin/rwDebug 0x3000
	sleep 2
	echo "··························································································"
done
