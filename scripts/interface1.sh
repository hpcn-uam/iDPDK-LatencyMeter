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
make && \
#make -j5 && \
\
        # c = numero de procesadores
        # n = numero de canales de memoria
        # --rx "(PORT, QUEUE, LCORE), ..." : List of NIC RX ports and queues       
        # tx "(PORT, QUEUE, LCORE), ..." : List of NIC TX ports handled by the I/O TX   
        # w "LCORE, ..." : List of the worker lcores
        # OPTIONAL:
        # rsz "A, B, C, D" : Ring sizes
        #   A = Size (in number of buffer descriptors) of each of the NIC RX    
        #       rings read by the I/O RX lcores (default value is 1024)           
        #   B = Size (in number of elements) of each of the SW rings used by the
        #       I/O RX lcores to send packets to worker lcores (default value is
        #       1024)
        #   C = Size (in number of elements) of each of the SW rings used by the
        #       worker lcores to send packets to I/O TX lcores (default value is
        #       1024)
        #   D = Size (in number of buffer descriptors) of each of the NIC TX    
        #       rings written by I/O TX lcores (default value is 1024)            
        # bsz "(A, B), (C, D), (E, F)" :  Burst sizes
        #   A = I/O RX lcore read burst size from NIC RX (default value is 144)  
        #   B = I/O RX lcore write burst size to output SW rings (default value 
        #       is 144)
        #   C = Worker lcore read burst size from input SW rings (default value 
        #       is 144)
        #   D = Worker lcore write burst size to output SW rings (default value 
        #       is 144)
        #   E = I/O TX lcore read burst size from input SW rings (default value 
        #       is 144)
        #   F = I/O TX lcore write burst size to NIC TX (default value is 144)   
\
        sudo build/app/hpcn_latency --socket-mem 8192,8192 --file-prefix iface1 -c FFFF0 -n 4 -w 0000:19:00.1 -- --rx "(0,0,18)" --tx "(0,0,19)" \
                --rsz "1024, 1024" \
                --bsz "144, 144" \
		--trainSleep 0 $@
