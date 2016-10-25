#!/bin/bash

#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#Script to execute
SCRIPT="$DIR/../interface01.sh $@ 2>> stderr.trash"

#awk Script parse
LOSSES=$($SCRIPT | grep lost | cut -f1 -d' ')
$SCRIPT | grep ': L' | grep -v "inf" | awk -v losses=$LOSSES -f $DIR/outputParser.awk | sed -e 's/\./,/g'