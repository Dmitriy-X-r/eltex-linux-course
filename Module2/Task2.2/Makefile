CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = calculator
SOURCES = main.c calculator.c
HEADERS = calculator.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
