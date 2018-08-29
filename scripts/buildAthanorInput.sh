#!/usr/bin/env bash
#arg 1 essence file,
#arg 2 param file optional 
#make temp file, concat arg2 then arg1 to temp file, invoke Conjure with output json and print
if [ $# -eq 2 ]
then file=mktemp
    trap "rm -f $file" EXIT

    cat $2 >> $file
    cat $1 | grep -vi 'language essence' >> $file
elif [ $# -eq 1 ] 
then file="$1"
else echo "Error: $0: usage: $0 essence_file [param_file]" 1>&2
    exit 1
fi

conjure pretty $file --output-format json