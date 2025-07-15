OBJDIR := obj
OBJDIR_DEBUG := obj_debug

CFLAGS += -std=c11 -O3 -g -Wall -Wmissing-declarations -D_DEFAULT_SOURCE -fpic -fno-common
CFLAGS_DEBUG += -std=c11 -O0 -g -Wall -Wmissing-declarations -D_DEFAULT_SOURCE -fpic -fno-common

LIBS += $(shell pkg-config --libs libusb-1.0) -lpthread
LIBS_DEBUG += $(shell pkg-config --libs libusb-1.0) -lpthread -lm

%.o: %.c | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(addprefix $(OBJDIR)/, $@) -I lpcsdr_firmware/include

%.o: %.c | $(OBJDIR_DEBUG)
	$(CC) $(CPPFLAGS) $(CFLAGS_DEBUG) -c $< -o $(addprefix $(OBJDIR_DEBUG)/, $@) -I lpcsdr_firmware/include

$(OBJDIR):
	mkdir -p $@

$(OBJDIR_DEBUG):
	mkdir -p $@

test: test.o errors.o boot.o config.o external.o device.o dsp.o
	$(CC) -g -o $@ $(addprefix $(OBJDIR)/, $^) $(LDFLAGS) $(LIBS)

debug: test.o errors.o boot.o config.o external.o device.o dsp.o
	$(CC) -g -o $@ $(addprefix $(OBJDIR_DEBUG)/, $^) $(LDFLAGS) $(LIBS_DEBUG)

libsdr.a: errors.o boot.o config.o external.o device.o dsp.o
	rm -f $@
	$(AR) rcs $@ $(addprefix $(OBJDIR)/, $^)

clean:
	rm -f libsdr.a test debug
	rm -rf $(OBJDIR)
	rm -rf $(OBJDIR_DEBUG)