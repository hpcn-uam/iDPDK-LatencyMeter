#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="60 128 1514"
TRAINLENS="$(seq 100 100 100000)"
TRAINSLEEPS="1500 150000"
TRAINFRIENDS="0"

for trainfriends in $TRAINFRIENDS ; do
    export RESULTBASE="$DIR/results/independency/$trainfriends/"
    for trainsleep in $TRAINSLEEPS ; do
        for trainlen in $TRAINLENS ; do
            for pktsize in $PKTSIZES ; do
                OUTPARAMS="--trainLen $trainlen --trainSleep $trainsleep --pktLen $pktsize --trainFriends $trainfriends"
                #echo "Test Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                if [ -f "$RESULTBASE/$OUTPARAMS.txt" ]; then
                    ISDOWN=$(grep "Link Down" -c "$RESULTBASE/$OUTPARAMS.txt")
                    ISCOMPLETED=$(grep "Mean" -c "$RESULTBASE/$OUTPARAMS.txt")
                    if [ "$ISDOWN" -gt "0" ]; then
                        echo "Down-link File Friend=$trainfriends Sleep=$trainsleep Train=$trainlen Len=$pktsize"
                        rm "$RESULTBASE/$OUTPARAMS.txt"
                    fi
                    if [ "$ISCOMPLETED" -lt "2" ]; then
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