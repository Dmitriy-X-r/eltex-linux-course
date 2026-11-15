CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -D_POSIX_C_SOURCE=200809L

TARGET = pubsub
OBJECTS = main.o broker.o publisher.o subscriber.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
