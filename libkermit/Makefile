TARGET_LIB = libkermit.a
OBJS = kermit.o sha1.o

CFLAGS = -G0 -Wall -O2
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)
	@echo "Type 'make install' to install the library!"

install: $(TARGET_LIB)
	@cp $(TARGET_LIB) $(PSPDEV)/psp/lib
	@cp kermit.h $(PSPDEV)/psp/include
	@echo "$(TARGET_LIB) installed!"

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
