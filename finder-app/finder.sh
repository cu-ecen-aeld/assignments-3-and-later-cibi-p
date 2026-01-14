#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Requires two arguments"
  exit 1
fi

FILEDIR=$1
SEARCHSTR=$2

if [ ! -d $FILEDIR ]; then
  echo "$1, Directory not found"
  exit 1
fi

Totalfiles=$(find $FILEDIR -type f | wc -l)
MatchedLines=$(grep -r $SEARCHSTR $FILEDIR | wc -l)

echo "The number of files are $Totalfiles and the number of matching lines are $MatchedLines"
