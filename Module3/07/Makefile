CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

CLIENT = chat
SERVER = chat_server

all: $(CLIENT) $(SERVER)

$(CLIENT): main.o chat.o
	$(CC) $(CFLAGS) -o $(CLIENT) main.o chat.o

$(SERVER): server_main.o server.o
	$(CC) $(CFLAGS) -o $(SERVER) server_main.o server.o

main.o: main.c chat.h
	$(CC) $(CFLAGS) -c main.c -o main.o

chat.o: chat.c chat.h
	$(CC) $(CFLAGS) -c chat.c -o chat.o

server_main.o: server_main.c chat.h
	$(CC) $(CFLAGS) -c server_main.c -o server_main.o

server.o: server.c chat.h
	$(CC) $(CFLAGS) -c server.c -o server.o

clean:
	rm -f $(CLIENT) $(SERVER) *.o

.PHONY: all clean
