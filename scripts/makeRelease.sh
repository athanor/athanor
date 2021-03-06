#!/usr/bin/env bash
#arg 1: destination directory
#arg 2 tag ref
tagRef="$2"
#arg 3: os, will be used as suffix
os="$3"
#arg 4: conjure exec
conjureExec="$4"

tag="${tagRef#"refs/tags/"}"
destDir="$1/athanor_${tag}_$os"
scriptDir="$(dirname "$0")"

mkdir "$destDir"
cp "$scriptDir/../build/release/athanor" "$destDir"
cp "$conjureExec" "$destDir"
cp "$scriptDir/../LICENSE" "$destDir"
cp "$scriptDir/manualBuildAthanorInput.sh" "$destDir"
cp "$scriptDir/../readme.md" "$destDir"
cd "$1"
tar -czvf "athanor_${tag}_${os}.tar.gz" "athanor_${tag}_${os}"