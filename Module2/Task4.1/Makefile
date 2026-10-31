CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = phonebook
OBJS = main.o phonebook.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c phonebook.h
	$(CC) $(CFLAGS) -c main.c

phonebook.o: phonebook.c phonebook.h
	$(CC) $(CFLAGS) -c phonebook.c

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
