clean:
	rm -rf build

build:
	mkdir build
	cmake -S ./src -B ./build
	cd build
	make