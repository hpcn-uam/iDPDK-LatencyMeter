#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PKTSIZES="$(seq 60 1514)"
TRAINLENS="10000"

for trainlen in $TRAINLENS ; do
    rm -f results.$trainlen.txt
    for pktsize in $PKTSIZES ; do
        echo "Test $trainlen $pktsize"
        $DIR/scriptExecuter.sh --trainLen $trainlen --pktLen $pktsize --sts | tee -a results.$trainlen.txt
    done
done