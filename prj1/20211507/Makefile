CC = gcc
CFLAGS = -g -Wall
OBJS = main.o list.o hash.o bitmap.o hex_dump.o
TARGET = testlib

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

list.o: list.h
hash.o: hash.h
bitmap.o: bitmap.h limits.h round.h hex_dump.h
hex_dump.o: hex_dump.h

clean:
	rm -f *.o
	rm -f $(TARGET)