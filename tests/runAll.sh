#!/usr/bin/env bash
numberIterations=10000

pushd $(dirname "$0") > /dev/null
trap "popd > /dev/null" EXIT

echo "Compiling..."
make -C .. athanorDebugMode -j8
if [ "$?" -ne 0 ]
then echo "Compiling failed. Exiting tests." 1>&2
    exit 1
fi

#Remember where this src folder is
solver="$PWD/../build/debug/athanor"

failedInstances=0
numberInstances=0

function markFailed() {
    ((failedInstances += 1))
    printf '\a'
    echo "***Failed, $2" 1>& 2
    mv "$outputDir" output/failed-"${outputDir#"output/"}"
}

function markPassed() {
    mv "$outputDir" output/passed-"${outputDir#"output/"}"
}
seed=$RANDOM

for instance in instances/*.essence ; do
    ((numberInstances += 1))
    echo "Running test $instance with seed $seed"
    outputDir="output/$(basename "$instance" .essence)-$seed"
    mkdir -p "$outputDir"
    "$solver" --sanityCheck --randomSeed $seed --iterationLimit $numberIterations --spec $instance &> "$outputDir/solver-output.txt"
    if [ $? -ne 0 ]
    then markFailed "$outputDir" "Solver had non 0 exit code.  Could be sanity check error."
        continue
    fi
    solsDir="$outputDir/sols"
    mkdir -p "$solsDir"
    if ! cat "$outputDir/solver-output.txt" | ../scripts/validateSolutions.py "$solsDir" conjure $instance &> "$outputDir/validator-output.txt"
    then markFailed "outputDir" "validate solutions failed"
        continue
    fi
    postCheck=$(dirname "$instance")/$(basename "$instance" .essence)-post-check.sh
    if [ -e "$postCheck" ]
    then if ! "$postCheck"  "$outputDir/solver-output.txt" &> "$outputDir/post-check.txt"
            then markFailed "outputDir" "post check failed."
            continue
        fi
    fi
    markPassed "$outputDir"    
done

if (( failedInstances > 0 )); then
    echo "Number of failed instances: $failedInstances" 1>&2 
else
    echo "All instances passed."
fi
echo "Number instances run: $numberInstances."


