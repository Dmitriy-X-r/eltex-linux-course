CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

TARGET = queue
SOURCES = main.c priority_queue.c
HEADERS = priority_queue.h

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
