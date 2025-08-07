#Set these to your folders
LPCSDR_DIR=/media/psf/soapy_shared_folder/liblpcsdr/build/src
LIB_HEADERS=/media/psf/soapy_shared_folder/liblpcsdr/include
FIRM_HEADERS=/media/psf/soapy_shared_folder/liblpcsdr/lpcsdr_firmware/include
export SOAPY_SDR_PLUGIN_PATH=/media/psf/soapy_shared_folder/liblpcsdr/soapy

CXXFLAGS += -std=c++11 -O3 -g -Wall -Werror -D_DEFAULT_SOURCE
LDFLAGS += -g -fpic -shared

CXXFLAGS += $(shell pkg-config --cflags SoapySDR)
LIBS += $(shell pkg-config --libs SoapySDR)

CXXFLAGS += $(shell pkg-config --cflags libusb-1.0) -I$(LPCSDR_DIR) -I$(LIB_HEADERS) -I$(FIRM_HEADERS) -fpic 
LIBS += -L$(LPCSDR_DIR) -lsdr $(shell pkg-config --libs libusb-1.0) -lpthread

all: libsdrSupport.so

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

libsdrSupport.so: soapy-lpcsdr.o
	$(CXX) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o libsdrSupport.so

format:
	clang-format -style=file -i *.cc *.h
