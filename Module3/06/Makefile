CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = chat
OBJECTS = main.o chat.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

main.o: main.c chat.h
	$(CC) $(CFLAGS) -c main.c -o main.o

chat.o: chat.c chat.h
	$(CC) $(CFLAGS) -c chat.c -o chat.o

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
