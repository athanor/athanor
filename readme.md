# __ATHANOR__
## Automated, scalable, local search of constraint satisfaction and constraint optimisation problems written in __Essence__
* [Why use __ATHANOR__](#why)
* [Using __ATHANOR__](#using)
* [Building from source](#installation)

# <a name="why"></a> Why use __ATHANOR__

## Powerful specifications:

__ATHANOR__ is a local search solver for constraint problems expressed in the __Essence__ language.  __Essence__ allows for the specification of constraint problems using high level mathematical types -- such as `set`, `sequence` and `partition` -- which are not available in other constraint modelling languages.  These high level types can be arbitrarily nested -- for example, `set of partition`, `set of sequence of tuple of ...`, etc.  The high level nature of __Essence__ allows for very concise specifications of constraint problems as the user is not required to make many modelling decisions. For example, how a `set of sequence of tuple (int, int)` should be represented.

## Performance:

The information conveyed by the high level __Essence__ types allow for a more performant and more scalable local search.  These benefits are described in a paper being published in The 2019 proceedings of the International Joint Conference for Artificial Intelligence.  In this paper, we benchmark __ATHANOR__ against six state of the art local search constraint solvers.  A version can be viewed here [link coming soon].

In short:

* Performance: __ATHANOR__ uses the high level __Essence__ types to derive more semantically meaningful neighbourhoods for its local search.
* Scalability: __ATHANOR__ understands that structures such as `sets`, `multi-sets` and `sequences`  can have a variable cardinality during search. Hence, unlike traditional solvers, which must pre allocate memory to support every structure reaching its maximum size, __ATHANOR__ dynamically allocates and de-allocates memory during search according to the cardinality of the structures.

## Multiple solvers:

Conjure is a great automated constraint modelling tool that accepts __Essence__ as an input language.  It is part of a toolchain that can automatically refine high level __Essence__ specifications into an input suitable for a range of solvers including CP, SAT, and more.  Find out more [here](https://conjure.readthedocs.io/en/latest).


# <a name="using"></a> Using __ATHANOR__

## __Essence__

* Typically, __Essence__ problems are divided into a specification file and a parameter file.  An __Essence__ specification describes the problem class -- given some input `X`, the desired output should be `Y`.  The parameter file is then used to specify a particular instance of `X`. 
* An __Essence__ specification normally consists of
    1. Givens: descriptions of the input domains,
    1. Finds: declaration of decision variables, variables used to encode solutions to the problem.
    1. Constraints: constraints that the values assigned to the decision variables must satisfy.
    1. Objective: an expression to minimise or maximise.  This may be an integer or a tuple of integers which is lex ordered.
* The parameter file is just a set of constants specified using *lettings*.
* _Getting started_, some example essence problems that work with __ATHANOR__ [coming soon].
* Read __Essence__ documentation [here](https://conjure.readthedocs.io/en/latest/essence.html). __ATHANOR__ operates on a subset of __Essence__.  *Feature requests are very welcome!*  Supported types: `int, bool, tuple, set, multi-set, sequence, function (total), partition (regular, fixed number parts)`.

## Obtaining __ATHANOR__

* Obtain the latest __ATHANOR__ Mac or Linux releases [here](https://github.com/athanor/athanor/releases). 
* Manual installation instructions [here](#installation).
* __ATHANOR__ depends on [Conjure](https://github.com/conjure-cp/conjure) to parse __Essence__. A __Conjure__ executable is bundled with the releases and __ATHANOR__ should be able to appropriately locate it.  

## Basic usage:
* `./athanor --spec path_to_essence_spec [--param path_to_essence_param]`
*  __Essence__ specifications are usually split into two parts, the specification of the problem and the parameters to the problem.
*  __ATHANOR__ invokes __Conjure__ to translate the given files into a JSON representation.
    * If you wish to skip this step when invoking __ATHANOR__, the translation can be manually invoked through __Conjure__ before running __ATHANOR__.  a bash script `manualBuildAthanorInput.sh` has been included to help.


# <a name="installation"></a> Building from source: 

The latest versions of __ATHANOR__ can be installed manually.

## Requirements:
* GNU Make
* cmake 3.0 onwards.
* C++14 compatible compiler.
    * clang:  According to [clang's documentation](https://clang.llvm.org/cxx_status.html), c++14 is supported with versions of clang 3.4 and later.  However, __ATHANOR__ has only been tested on clang 5 onwards.
    * gcc: Unfortunately, due to internal compiler bugs, only the newest gcc compilers are compatible with __ATHANOR__.  **Use gcc 8 onwards**.
* [Conjure](https://github.com/conjure-cp/conjure), (for running __ATHANOR__ after installation). Conjure must either be in the path or be located next to the __ATHANOR__ executable.

## Installation:
### Quick install:
```
git clone https://github.com/__ATHANOR__/__ATHANOR__
cd __ATHANOR__
make
```
For parallel build, `make -jn` replacing n with number of cores.

*Note*, you can specify the compiler to use by writing `export CXX=PUT_COMPILER_COMMAND_HERE`.  For example `export CXX=clang++`.

The binary `athanor` will be placed in `<pwd>/build/release/athanor`
    
### Manual install:
```
git clone https://github.com/__ATHANOR__/__ATHANOR__
cd __ATHANOR__
```
Now additional libraries need to be pulled from github.
```
git submodule init
git submodule update
```

Then make a build directory and build __ATHANOR__ there

```
mkdir -p build/release
cd build/release
cmake ../..
make
```
For parallel build, `make -jn` replacing n with number of cores.

*Note*, you can specify the compiler to use by writing `export CXX=PUT_COMPILER_COMMAND_HERE`.  For example `export CXX=clang++`.

