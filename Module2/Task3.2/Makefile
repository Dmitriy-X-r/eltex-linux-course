
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = subnet
SOURCES = main.c subnet.c
HEADERS = subnet.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
