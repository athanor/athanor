#!/usr/bin/env bash


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

function checkExitStatus() {
    if [ "$exitStatus" -ne 0 ]
    then markFailed "$outputDir" "Solver had non 0 exit code.  Could be sanity check error."
        return 1
    fi
    return 0
}

function validateSolutions() {
    solsDir="$outputDir/sols"
    mkdir -p "$solsDir"
    if ! cat "$outputDir/solver-output.txt" | ../scripts/validateSolutions.py "$solsDir" conjure "$instance" &> "$outputDir/validator-output.txt"
        then if grep 'No solutions found' "$outputDir/validator-output.txt"  > /dev/null && grep -E '^\$testing:no-solutions' "$instance" > /dev/null
            then return 0
            fi
            markFailed "outputDir" "validate solutions failed"
        return 1
    fi
    return 0
}

function runPostChecks() {
    postCheck=$(dirname "$instance")/$(basename "$instance" .essence)-post-check.sh
    if [ -e "$postCheck" ]
    then if ! "$postCheck"  "$outputDir/solver-output.txt" &> "$outputDir/post-check.txt"
            then markFailed "outputDir" "post check failed."
                return 1
        fi
    fi
    return 0
}

for instance in instances/*.essence ; do
    ((numberInstances += 1))
    echo "Running test $instance with seed $seed"
    outputDir="output/$(basename "$instance" .essence)-$seed"
    mkdir -p "$outputDir"
    numberIterations=$(grep -E '^\$testing:numberIterations=' "$instance" | grep -Eo '[0-9]+')
    "$solver" --sanityCheck --randomSeed $seed --iterationLimit $numberIterations --spec $instance &> "$outputDir/solver-output.txt"
    exitStatus=$?
    checkExitStatus &&
    validateSolutions &&
    runPostChecks &&
    markPassed "$outputDir"    
done

if (( failedInstances > 0 )); then
    echo "Number of failed instances: $failedInstances" 1>&2 
else
    echo "All instances passed."
fi
echo "Number instances run: $numberInstances."

