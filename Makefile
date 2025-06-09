CFLAGS += -std=c11 -O3 -g -Wall -Wmissing-declarations -D_DEFAULT_SOURCE -fpic -fno-common
LIBS += $(shell pkg-config --libs libusb-1.0) -lpthread


%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@ -I lpcsdr_firmware/include

test: test.o errors.o boot.o config.o external.o device.o
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

libsdr.a: errors.o boot.o config.o external.o device.o
	rm -f $@
	$(AR) rcs $@ $^

clean:
	rm -f libsdr.a *.o