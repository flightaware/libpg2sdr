# Heard you like Makefiles. So I made a Makefile to call your CMake files

all: build

clean:
	rm -rf build

build:
	cmake -B ./build -S ./
	make -C build

run-unit-tests: build
	cd build && ctest -L "Unit" --output-on-failure

run-stream-integration-tests: build
	cd build && ctest -L "Integration" --output-on-failure

regenerate-starch:
	cd dsp && ./starchgen.py --runtime-dir . --output-dir generated

.PHONY: build clean run-unit-tests run-stream-integration-tests regenerate-starch
