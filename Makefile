all: athanor

athanor: build/Makefile
	@$(MAKE) -C build

athanorDebugMode: build/debug/Makefile
	@$(MAKE) -C build/debug



build/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update --depth=10
	mkdir -p build
	cd build ; export CXX=clang++ ; cmake ..


build/debug/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update --depth=10
	mkdir -p build/debug
	cd build/debug ; export CXX=clang++ ; cmake ../.. -DCMAKE_BUILD_TYPE=debug
