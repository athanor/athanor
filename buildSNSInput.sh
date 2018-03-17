#!/usr/bin/env bash

#arg 1 essence file,
#arg 2 param file
#make temp file, concat arg2 then arg1 to temp file, invoke Conjure with output json and print

conjure pretty <(cat $2 ; cat $1) --output-format json