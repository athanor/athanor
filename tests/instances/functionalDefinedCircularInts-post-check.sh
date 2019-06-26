pushd > /dev/null
numberIterations=$(../utils/getNumberIterations.sh "$1")
if ! (( numberIterations == 0 ))
then echo "Error number iterations should be 0 but is instead $numberIterations"
    exit 1
fi
