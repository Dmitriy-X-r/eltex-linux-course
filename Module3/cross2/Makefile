CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = taxi
OBJECTS = main.o taxi.o driver.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

main.o: main.c taxi.h
	$(CC) $(CFLAGS) -c main.c -o main.o

taxi.o: taxi.c taxi.h driver.h
	$(CC) $(CFLAGS) -c taxi.c -o taxi.o

driver.o: driver.c driver.h taxi.h
	$(CC) $(CFLAGS) -c driver.c -o driver.o

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
