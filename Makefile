# Heard you like Makefiles. So I made a Makefile to call your CMake files

clean:
	rm -rf local
	rm -rf debug

build:
	cmake -B ./local
	cd local && make

debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -B ./debug
	cd debug && make