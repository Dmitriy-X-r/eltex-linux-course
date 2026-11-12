
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = filecopy
OBJECTS = main.o ipc.o filecopy.o

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

main.o: main.c ipc.h
	$(CC) $(CFLAGS) -c main.c -o main.o

ipc.o: ipc.c ipc.h filecopy.h
	$(CC) $(CFLAGS) -c ipc.c -o ipc.o

filecopy.o: filecopy.c filecopy.h
	$(CC) $(CFLAGS) -c filecopy.c -o filecopy.o

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
