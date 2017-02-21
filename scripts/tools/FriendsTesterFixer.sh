#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="60 64 70 80 90 98 100 128 256 512 1024 1514"
TRAINLENS="1000"
TRAINSLEEPS="$(seq 0 50 1500)"
TRAINFRIENDS="$(seq 0 9) $(seq 10 5 70)"

for trainfriends in $TRAINFRIENDS ; do
    export RESULTBASE="$DIR/results/friendly/$trainfriends/"
    for trainsleep in $TRAINSLEEPS ; do
        for trainlen in $TRAINLENS ; do
            for pktsize in $PKTSIZES ; do
                OUTPARAMS="--trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends $trainfriends"
                #echo "Test Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                if [ -f "$RESULTBASE/$OUTPARAMS.txt" ]; then
                    ISDOWN=$(grep "Link Down" -c "$RESULTBASE/$OUTPARAMS.txt")
                    if [ "$ISDOWN" -gt "0" ]; then
                        echo "Down-link File Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                        rm "$RESULTBASE/$OUTPARAMS.txt"
                    fi
                    ISCOMPLETED=$(grep "Mean" -c "$RESULTBASE/$OUTPARAMS.txt")
                    if [ "$ISCOMPLETED" -lt "0" ]; then
                        echo "Incomplete File Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                        rm "$RESULTBASE/$OUTPARAMS.txt"
                    fi
                else
                    echo "Redoing test (not exists) Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                    $DIR/scriptExecuter.sh --trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends $trainfriends
                    rm -f $RESULTBASE/results.$trainsleep.$trainlen.txt
                fi
            done
        done
    done
done