# Heard you like Makefiles. So I made a Makefile to call your CMake files

clean:
	rm -rf build

build:
	make clean
	cmake -B ./build -S ./
	cd build && make

debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -B ./build -S ./
	cd build && make

run-tests:
	make clean
	make build
	cd build && ctest --output-on-failure

dsp:
	make clean
	make debug
	cd build/tests &&  gdb dsp_tests.exe

adc:
	make clean
	make debug
	cd build/tests &&  gdb adc_tests.exe

str:
	make clean
	make debug
	cd build/tests &&  gdb stream_tests.exe
