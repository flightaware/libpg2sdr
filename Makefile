# Heard you like Makefiles. So I made a Makefile to call your CMake files

all: build

clean:
	rm -rf build

build:
	cmake -B ./build -S ./ -DENABLE_CLI=ON -DENABLE_SOAPYSDR=ON -DENABLE_TESTING=OFF -DCMAKE_VERBOSE_MAKEFILE=OFF
	cmake --build ./build

verbose:
	cmake -B ./build -S ./ -DENABLE_CLI=ON -DENABLE_SOAPYSDR=ON -DENABLE_TESTING=OFF -DCMAKE_VERBOSE_MAKEFILE=ON
	cmake --build ./build

build-with-tests:
	cmake -B ./build -S ./ -DENABLE_CLI=ON -DENABLE_SOAPYSDR=ON -DENABLE-TESTING=ON -DCMAKE_VERBOSE_MAKEFILE=OFF
	cmake --build ./build

run-unit-tests: build-with-tests
	cd build && ctest -L "Unit" --output-on-failure

run-stream-integration-tests: build-with-tests
	cd build && ctest -L "Integration" --output-on-failure

regenerate-starch:
	cd dsp && ./starchgen.py --runtime-dir . --output-dir generated

.PHONY: build clean verbose build-with-tests run-unit-tests run-stream-integration-tests regenerate-starch
