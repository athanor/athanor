all: athanor

athanor: build/Makefile
	@$(MAKE) -C build


build/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update --depth=10
	mkdir -p build
	cd build ; export CXX=clang++ ; cmake ..