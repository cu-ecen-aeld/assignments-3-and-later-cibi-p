#!/bin/bash

if [ $# -ne 2 ]; then
  echo "Requires two arguments"
  exit 1
fi

FILEPATH=$1
WRITESTRING=$2

DIRPATH=$(dirname $FILEPATH)
if [ ! -d $DIRPATH ]; then
  mkdir "$DIRPATH" -p
fi

echo $WRITESTRING > "$FILEPATH"
