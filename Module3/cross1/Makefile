CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

.PHONY: all clean

all: echo_server echo_client

echo_server: main.o server.o packet.o
	$(CC) $(CFLAGS) -o $@ $^

echo_client: client_main.o client.o packet.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c server.h protocol.h
server.o: server.c server.h packet.h protocol.h
client_main.o: client_main.c client.h
client.o: client.c client.h packet.h protocol.h
packet.o: packet.c packet.h protocol.h

clean:
	rm -f *.o echo_server echo_client
