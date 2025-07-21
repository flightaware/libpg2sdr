# Heard you like Makefiles. So I made a Makefile to call your CMake files

clean:
	rm -rf local_build
	rm -rf debug

build:
	cmake -B ./local_build
	cd local_build && make

debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -B ./debug
	cd debug && make

run-tests:
	make build
	cd local_build && ctest --output-on-failure
