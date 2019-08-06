all : athanor

          athanor : build /
                    release / Makefile @$(MAKE) -
    C build /
        release

        athanorDebugMode : build
                           /
                           debug / Makefile @$(MAKE) -
    C build /
        debug

        build
        / release /
        Makefile : @echo "Fetching libs from github..." git submodule init git
                   submodule update mkdir
    - p build / release cd build / release;
cmake../
            ..

            build /
        debug /
        Makefile : @echo "Fetching libs from github..." git submodule init git
                       submodule update mkdir -
    p build / debug cd build / debug;
cmake../..- DCMAKE_BUILD_TYPE = debug

                                    clean : rm -
                                            rf build