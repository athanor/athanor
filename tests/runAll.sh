#!/usr/bin/env bash
if [ $# -ne 01 ]
then echo "Error, requires one argument, number of iterations" 1>&2
    exit 1
fi
numberIterations=$1
pushd $(dirname "$0") > /dev/null
trap "popd > /dev/null" EXIT

echo "Compiling..."
cd ../build/release
cmake ../.. && make -j4
if [ "$?" -ne 0 ]
then echo "Compiling failed. Exiting tests." 1>&2
    exit 1
fi

#Remember where this src folder is
solver="$PWD/athanor"
cd - > /dev/null

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
    "$solver" --sanityCheck --randomSeed $seed --iterationLimit $numberIterations --spec $instance &> "$outputDir/solver_output.txt"
    if [ $? -ne 0 ]
    then markFailed "$outputDir" "Solver had non 0 exit code.  Could be sanity check error."
        continue
    fi
    solsDir="$outputDir/sols"
    mkdir -p "$solsDir"
    if ! cat "$outputDir/solver_output.txt" | ../scripts/validateSolutions.py "$solsDir" conjure $instance &> "$outputDir/validator_output.txt"
    then markFailed "outputDir" "validate solutions failed"
        continue
    fi
markPassed "$outputDir"    
done

if (( failedInstances > 0 )); then
    echo "Number of failed instances: $failedInstances" 1>&2 
else
    echo "All instances passed."
fi
echo "Number instances run: $numberInstances."


