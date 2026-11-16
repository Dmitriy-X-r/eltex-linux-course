CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g
LDFLAGS = -pthread -lrt

TARGET = chat
OBJECTS = main.o chat.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

main.o: main.c chat.h
	$(CC) $(CFLAGS) -c main.c -o main.o

chat.o: chat.c chat.h
	$(CC) $(CFLAGS) -c chat.c -o chat.o

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
