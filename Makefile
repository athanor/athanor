all: athanor

athanor: build/Makefile
	make -C build


build/Makefile:
	@echo "Fetching libs from github..."
	git submodule init
	git submodule update --depth=10
	mkdir -p build
	cd build ; cmake ..