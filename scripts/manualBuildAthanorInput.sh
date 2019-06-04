#!/usr/bin/env bash
#arg 1 essence file,

if [ $# -eq 1 ] 
then file="$1"
else echo "Error: $0: usage: $0 essence_file" 1>&2
    exit 1
fi

conjure pretty $file --output-format json