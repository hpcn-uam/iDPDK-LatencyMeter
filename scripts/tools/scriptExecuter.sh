#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#Script to execute
SCRIPT="$DIR/../interface01.sh $@ 2>> stderr.trash"

FNAME="$DIR/results/$@.txt"
mkdir -p "$DIR/results"

#awk Script parse
$SCRIPT > "$FNAME"
LOSSES=$(cat "$FNAME" | grep lost | cut -f1 -d' ')
cat "$FNAME" | grep ': L' | grep -v "inf" | awk -v losses=$LOSSES -f $DIR/outputParser.awk | sed -e 's/\./,/g'