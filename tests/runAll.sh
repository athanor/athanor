#!/usr/bin/env bash
#to skip sanity check which is slow!, pass arg skipSanityCheck 
#to use release build pass useReleaseBuild
# to pass a bash expansion filter for filtering instances, like you would pass to ls, then use --filter pattern
pushd $(dirname "$0") > /dev/null
trap "popd > /dev/null" EXIT


sanityCheckFlag="--sanity-check"
useReleaseBuild=0
dumpFilesOnError=0
numberHeadTailSolutions=15
instanceFilter='instances/*.essence'
skip=0
while (($# > 0)) ; do
    flag="$1"
    if [[ "$flag" == "skipSanityCheck" ]] ; then
        sanityCheckFlag=""
        echo "Skipping sanity check"
    elif [[ "$flag" == "useReleaseBuild" ]] ; then
        useReleaseBuild=1
        echo "using release build"
    elif [[ "$flag" == "dumpFilesOnError" ]] ; then
        dumpFilesOnError=1
        echo "dump files on error is on"
    elif [[ "$flag" == "--filter" ]] ; then
        if (($# < 2 )) ; then 
            echo "Error --filter takes one argument, a bash pattern", see test docs."" 
            exit 1
        fi
        instanceFilter="$2"
        echo "instanceFilter=$instanceFilter"
        shift
    else 
        echo "bad argument, see test docs." 1>&2
        exit 1
    fi
    shift
done



echo "Compiling..."
if [[ "$useReleaseBuild" -eq 0 ]] ; then
    make -C .. athanorDebugMode -j8
else
    make -C .. -j8
fi
if [ "$?" -ne 0 ]
then echo "Compiling failed. Exiting tests." 1>&2
    exit 1
fi

#Remember where this src folder is
if [ "$useReleaseBuild" -eq 0 ] ; then
    solver="$PWD/../build/debug/athanor"
    disableDebugLogFlag="--disable-debug-log"
else
    solver="$PWD/../build/release/athanor"
    disableDebugLogFlag=""
fi


failedInstances=0
numberInstances=0


function maybeDump() {
    if [ "$dumpFilesOnError" -ne 0 ] ; then
        cat "$1"
    fi
}
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
    if [ "$exitStatus" -ne 0 ] ; then
        maybeDump "$outputDir/solver-output.txt"
        markFailed "$outputDir" "Solver had non 0 exit code.  Could be sanity check error."
        return 1
    fi
    return 0
}

function validateSolutions() {
    solsDir="$outputDir/sols"
    mkdir -p "$solsDir"
    if [ "$withParam" -eq 1 ]
    then validateCommand=(../scripts/validateSolutions.py $numberHeadTailSolutions "$solsDir" conjure "$instance" "$param")
    else validateCommand=(../scripts/validateSolutions.py $numberHeadTailSolutions "$solsDir" conjure "$instance")
    fi
    if ! cat "$outputDir/solver-output.txt" | runCommand  "$outputDir/validator-output.txt" "${validateCommand[@]}" 
        then if grep 'No solutions found' "$outputDir/validator-output.txt"  > /dev/null && grep -E '^\$testing:no-solutions' "$instance" > /dev/null
            then return 0
            fi
            maybeDump "$outputDir/validator-output.txt"
            markFailed "outputDir" "validate solutions failed"
        return 1
    fi
    return 0
}

function runPostChecks() {
    if [ "$withParam" -eq 1 ] ; then
        postCheck=$(dirname "$param")/$(basename "$param" .param)-post-check.sh
    else
        postCheck=$(dirname "$instance")/$(basename "$instance" .essence)-post-check.sh
    fi
    if [ -e "$postCheck" ]
    then if ! "$postCheck"  "$outputDir/solver-output.txt" &> "$outputDir/post-check.txt"
            then markFailed "outputDir" "post check failed."
                return 1
        fi
    fi
    return 0
}

function runCommand() {
    outputFile="$1"
    shift
    echo "pwd=$PWD" 2>&1 >> "$outputFile"
    echo "command=$@" >> "$outputFile"
    "$@" >> "$outputFile" 2>&1
}
for instance in $(eval ls $instanceFilter) ; do
    for param in instances/$(basename "$instance" .essence)*.param ; do
        ((numberInstances += 1))
        if [[ "$param" == "instances/$(basename "$instance" .essence)*.param" ]] ; then
            withParam=0
            echo "Running test $instance with seed $seed"
            outputDir="output/$(basename "$instance" .essence)-$seed"
            mkdir -p "$outputDir"
            numberIterations=$(grep -E '^\$testing:numberIterations=' "$instance" | grep -Eo '[0-9]+')
            runCommand "$outputDir/solver-output.txt" "$solver" $disableDebugLogFlag $sanityCheckFlag --random-seed $seed --iteration-limit $numberIterations  --spec "$instance" 
        else
            withParam=1
            echo "Running test $instance with param $param with seed $seed"
            outputDir="output/$(basename "$instance" .essence)-$(basename "$param" .param)-$seed"
            mkdir -p "$outputDir"
            numberIterations=$(grep -E '^\$testing:numberIterations=' "$param" | grep -Eo '[0-9]+')
            runCommand "$outputDir/solver-output.txt" "$solver" $disableDebugLogFlag $sanityCheckFlag --random-seed $seed --iteration-limit $numberIterations  --spec "$instance" --param "$param" 
        fi
        exitStatus=$?
        checkExitStatus &&
        validateSolutions &&
        runPostChecks &&
        markPassed "$outputDir"    
    done
done

if (( failedInstances > 0 )); then
    echo "Number of failed instances: $failedInstances" 1>&2 
else
    echo "All instances passed."
fi
echo "Number instances run: $numberInstances."



exit $failedInstances
