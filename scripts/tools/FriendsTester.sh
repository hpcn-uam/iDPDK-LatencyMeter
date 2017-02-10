#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="60 64 70 80 90 98 100 128 256 512 1024 1514"
TRAINLENS="1000"
TRAINSLEEPS="$(seq 0 10 2000)"
TRAINFRIENDS="$(seq 0 70)"

for trainfriends in $TRAINFRIENDS ; do
    export RESULTBASE=$DIR/results/friendly/$trainfriends/
    for trainsleep in $TRAINSLEEPS ; do
        for trainlen in $TRAINLENS ; do
            rm -f results.$trainsleep.$trainlen.txt
            for pktsize in $PKTSIZES ; do
                echo "Test $trainsleep $trainlen $pktsize"
                $DIR/scriptExecuter.sh --trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends 70 | tee -a $RESULTBASE/results.$trainsleep.$trainlen.txt
            done
        done
    done
done