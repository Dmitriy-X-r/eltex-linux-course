CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = phonebook
LIBRARY = libphonebook.so

$(TARGET): main.o $(LIBRARY)
	$(CC) $(CFLAGS) -o $(TARGET) main.o -L. -lphonebook

$(LIBRARY): phonebook.o
	$(CC) -shared -o $(LIBRARY) phonebook.o

main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c

phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -fPIC -c phonebook.c

clean:
	rm -f $(TARGET) main.o phonebook.o $(LIBRARY)

.PHONY: clean
