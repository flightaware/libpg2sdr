# Heard you like Makefiles. So I made a Makefile to call your CMake files

all: build

clean:
	rm -rf build

build:
	cmake -B ./build -S ./
	make -C build

verbose:
	cmake -B ./build -S ./ -DCMAKE_VERBOSE_MAKEFILE=ON
	make -C build

build-tests:
	cmake -B ./build -S ./ -DENABLE_TESTING=ON
	make -C build

run-unit-tests: build-tests
	cd build && ctest -L "Unit" --output-on-failure

run-stream-integration-tests: build-tests
	cd build && ctest -L "Integration" --output-on-failure

regenerate-starch:
	cd dsp && ./starchgen.py --runtime-dir . --output-dir generated

.PHONY: build clean run-unit-tests run-stream-integration-tests regenerate-starch
