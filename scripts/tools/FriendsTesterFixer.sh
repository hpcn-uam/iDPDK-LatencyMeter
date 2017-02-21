#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="60 64 70 80 90 98 100 128 256 512 1024 1514"
TRAINLENS="1000"
TRAINSLEEPS="$(seq 0 50 1500)"
TRAINFRIENDS="$(seq 0 70)"

for trainfriends in $TRAINFRIENDS ; do
    export RESULTBASE="$DIR/results/friendly/$trainfriends/"
    for trainsleep in $TRAINSLEEPS ; do
        for trainlen in $TRAINLENS ; do
            for pktsize in $PKTSIZES ; do
                OUTPARAMS="--trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends $trainfriends"
                #echo "Test Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                if [ -f "$OUTPARAMS.txt" ]; then
                    touch /tmp/nothing
                else
                    echo "Redoing test (not exists) Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                    $DIR/scriptExecuter.sh --trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends $trainfriends
                    rm -f $RESULTBASE/results.$trainsleep.$trainlen.txt
                fi
            done
        done
    done
done