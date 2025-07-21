# liblpcsdr

## TODO

1. CMake should print out warnings and such
2. Cmake should dynamically find library name for linked libraries
3. Fix all the compiler warnings
4. Refactor Magic Numbers. Or at least, add comments.
4. README

# Development
export LPCSDR_FIRMWARE_PATH=/media/psf/soapy_shared_folder/liblpcsdr/lpcsdr_firmware/images/lpcsdr.bin 


## Initialize lpcsdr_firmware Submodule
`git submodule init`
`git submodule update`

# Unit Testing

We use Gtest. It relies on Cmake version ???