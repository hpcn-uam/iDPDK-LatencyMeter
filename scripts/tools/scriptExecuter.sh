#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#Script to execute
SCRIPT="$DIR/../interface01.sh $@ 2>> stderr.trash"

if [ -z "$RESULTBASE" ]; then
    RESULTBASE="$DIR/results/"
fi

FNAME="$RESULTBASE/$@.txt"
mkdir -p "$RESULTBASE"

#awk Script parse
$SCRIPT > "$FNAME"
LOSSES=$(cat "$FNAME" | grep lost | cut -f1 -d' ')
cat "$FNAME" | grep ': L' | grep -v "inf" | awk -v losses=$LOSSES -f $DIR/outputParser.awk | sed -e 's/\./,/g'