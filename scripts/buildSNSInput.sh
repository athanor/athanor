#!/usr/bin/env bash
#arg 1 essence file,
#arg 2 param file
#make temp file, concat arg2 then arg1 to temp file, invoke Conjure with output json and print
file=mktemp
trap "rm -f $file" EXIT

cat $2 >> $file
cat $1 | grep -v 'language Essence' >> $file
conjure pretty $file --output-format json