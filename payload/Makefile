release: all
	pcbctool -e $(TARGET).bin

TARGET = payload
OBJS = start.o main.o

CFLAGS = -O2 -G0 -Wall -fno-pic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lc
LDFLAGS = -nostartfiles -T linker.x

PSPSDK = $(shell psp-config --pspsdk-path)
include build.mak
