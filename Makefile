OBJDIR := obj

CFLAGS += -std=c11 -O3 -g -Wall -Wmissing-declarations -D_DEFAULT_SOURCE -fpic -fno-common
LIBS += $(shell pkg-config --libs libusb-1.0) -lpthread

%.o: %.c | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(addprefix $(OBJDIR)/, $@) -I lpcsdr_firmware/include

$(OBJDIR):
	mkdir -p $@

test: test.o errors.o boot.o config.o external.o device.o
	$(CC) -g -o $@ $(addprefix $(OBJDIR)/, $^) $(LDFLAGS) $(LIBS)

libsdr.a: errors.o boot.o config.o external.o device.o
	rm -f $@
	$(AR) rcs $@ $(addprefix $(OBJDIR)/, $^)

clean:
	rm -f libsdr.a test
	rm -rf obj