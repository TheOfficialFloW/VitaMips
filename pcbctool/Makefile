TARGET = pcbctool
OBJS = pcbctool.o kirk_engine.o crypto.o bn.o ec.o

CC = gcc
CFLAGS = -Wall -g -O0

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	$(RM) *.o $(TARGET) *.exe
