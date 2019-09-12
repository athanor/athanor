#!/usr/bin/env bash
#arg 1: destination directory
#arg 2: os, will be used as suffix
os="$2"
destDir="$1/$os"
#arg 3: conjure exec
conjureExec="$3"

scriptDir="$(dirname "$0")"

mkdir "$destDir"
cp "$scriptDir/../build/release/athanor" "$destDir"
cp "$conjureExec" "$destDir"
cp "$scriptDir/../LICENSE" "$destDir"
cp "$scriptDir/manualBuildAthanor.sh" "$destDir"
cp "$scriptDir/../readme.md" "$destDir"
tar -czvf "${destDir}.tar.gz" "$destDir"