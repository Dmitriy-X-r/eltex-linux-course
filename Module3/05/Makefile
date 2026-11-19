CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -pthread

TARGET = producer_consumer

all: $(TARGET)

$(TARGET): main.o ipc.o producer.o consumer.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o ipc.o producer.o consumer.o

main.o: main.c shared.h
	$(CC) $(CFLAGS) -c main.c -o main.o

ipc.o: ipc.c shared.h
	$(CC) $(CFLAGS) -c ipc.c -o ipc.o

producer.o: producer.c shared.h
	$(CC) $(CFLAGS) -c producer.c -o producer.o

consumer.o: consumer.c shared.h
	$(CC) $(CFLAGS) -c consumer.c -o consumer.o

clean:
	rm -f *.o $(TARGET)

.PHONY: all clean
