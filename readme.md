# Athanor
## Automated, scalable, local search of constraint satisfaction and constraint optimisation problems written in Essence

Details coming soon...


# Installation:
## Requirements:
* GNU Make
* cmake 3.0 onwards.
* C++14 compatible compiler.
    * clang:  According to [clang's documentation](https://clang.llvm.org/cxx_status.html), c++14 is supported with versions of clang 3.4 and later.  However, it has only been tested on clang 5 onwards.
    * gcc: Unfortunately, due to internal compiler bugs, only the newest gcc compilers are compatible with Athanor.  Use gcc 8 onwards.

## Installation:
### Quick install:
```
git clone https://github.com/athanor/athanor
cd athanor
make
```
For parallel build, `make -jn` replacing n with number of cores.

*Note*, you can specify the compiler to use by writing `export CXX=PUT_COMPILER_COMMAND_HERE`.  For example `export CXX=clang++`.

### Manual install:
```
git clone https://github.com/athanor/athanor
cd athanor
```
Now additional libraries need to be pulled from github.
```
git submodule init
git submodule update
```

Then make a build directory and build athanor there

```
mkdir build
cd build
cmake ..
make
```
For parallel build, `make -jn` replacing n with number of cores.

*Note*, you can specify the compiler to use by writing `export CXX=PUT_COMPILER_COMMAND_HERE`.  For example `export CXX=clang++`.

# Usage:
Athanor does not currently support direct parsing of Essence.  It requires a Essence to JSON translator, as provided by [conjure](https://github.com/conjure-cp/conjure).  Conjure is a much larger tool for automated modelling.  However, a script `script/buildAthanorInput.sh` is included in this repository which invokes the essence to json translator.  A stand alone parser will hopefully be included into Athanor soon.

Instructions TBC