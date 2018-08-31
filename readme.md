# Athanor
## Automated, scalable, local search of constraint satisfaction and constraint optimisation problems written in Essence

Details coming soon...


# Installation:
## Requirements:
* GNU Make
* cmake 3.0 onwards.
* C++14 compatible compiler.
* According to [clang's documentation](https://clang.llvm.org/cxx_status.html), c++14 is supported with versions of clang 3.4 and later.  However, it has only been tested on clang 5 onwards.  Further testing is being done with older versions.
*  Unfortunately, due to some internal compiler bugs, Athanor currently cannot be compiled with several versions of GCC.  Looking for work arounds.  The clang compiler is strongly recommended.

## Installation:
### Quick install:
```
git clone https://github.com/athanor/athanor
cd athanor
make
```
For parallel build, `make -jn` replacing n with number of cores.

### Manual install:
```
git clone https://github.com/athanor/athanor
cd athanor
```
Now additional libraries need to be installed.  If you are running on bash, run the script:
```
./fetchAllLibs.sh
```
Otherwise run the following:
```
git submodule init
git submodule update --depth=10
```

Then make a build directory and build athanor there

```
mkdir build
cd build
export CXX=clang++
cmake ..
make
```
For parallel build, `make -jn` replacing n with number of cores.

# Usage:
Athanor does not currently support direct parsing of Essence.  It requires a Essence to JSON translator, as provided by [conjure](https://github.com/conjure-cp/conjure).  Conjure is a much larger tool for automated modelling.  However, a script `script/buildAthanorInput.sh` is included in this repository which invokes the essence to json translator.  A stand alone parser will hopefully be included into Athanor soon.

Instructions TBC