#Set these to your folders
LPCSDR_DIR=../build/src
LIB_HEADERS=../include

CXXFLAGS += -std=c++14 -O3 -g -Wall -Werror -D_DEFAULT_SOURCE
LDFLAGS += -g -fpic -shared

CXXFLAGS += $(shell pkg-config --cflags SoapySDR)
LIBS += $(shell pkg-config --libs SoapySDR)

CXXFLAGS += $(shell pkg-config --cflags libusb-1.0) -I$(LPCSDR_DIR) -I$(LIB_HEADERS) -fpic 
LIBS += -L$(LPCSDR_DIR) -llpcsdr $(shell pkg-config --libs libusb-1.0) -lpthread

all: liblpcsdrSupport.so

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

liblpcsdrSupport.so: soapy-lpcsdr.o
	$(CXX) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o liblpcsdrSupport.so

format:
	clang-format -style=file -i *.cc *.h
