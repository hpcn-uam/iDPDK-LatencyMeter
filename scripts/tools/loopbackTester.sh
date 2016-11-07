#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="60 64 70 80 90 98 100 128 256 512 1024 1514"
TRAINLENS="100 1000 10000"
TRAINSLEEPS="0 64 128 1024"
WAITTIME="1000000000 10000000000"

for waittime in $WAITTIME ; do
    for trainsleep in $TRAINSLEEPS ; do
        for trainlen in $TRAINLENS ; do
            rm -f results_$waittime_$trainsleep_$trainlen.txt
            for pktsize in $PKTSIZES ; do
                echo "Test $waittime $trainsleep $trainlen $pktsize"
                $DIR/scriptExecuter.sh --trainLen $trainlen --trainSleep $trainsleep --waitTime $waittime --pktLen $pktsize | tee -a results.$waittime.$trainsleep.$trainlen.txt
            done
        done
    done
done