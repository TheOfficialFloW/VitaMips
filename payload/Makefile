release: all
	pcbctool -e $(TARGET).bin

TARGET = payload
OBJS = main.o

CFLAGS = -O2 -G0 -Wall -fno-pic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LDFLAGS = -nostartfiles -T linker.x -nostdlib

PSPSDK = $(shell psp-config --pspsdk-path)
include build.mak
