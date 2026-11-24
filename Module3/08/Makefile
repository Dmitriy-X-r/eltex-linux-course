CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = capture
OBJECTS = main.o capture.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

main.o: main.c capture.h
	$(CC) $(CFLAGS) -c main.c -o main.o

capture.o: capture.c capture.h chat.h
	$(CC) $(CFLAGS) -c capture.c -o capture.o

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean