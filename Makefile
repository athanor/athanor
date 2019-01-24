all: athanor

athanor: build/Makefile
	@$(MAKE) -C build

athanorDebugMode: build/debug/Makefile
	@$(MAKE) -C build/debug



build/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update
	mkdir -p build
	cd build ;  cmake ..


build/debug/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update
	mkdir -p build/debug
	cd build/debug ; cmake ../.. -DCMAKE_BUILD_TYPE=debug

clean:
	rm -rf build