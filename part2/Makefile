CC=g++

CFLAGS=-Wall -W -g -Werror



all: client server

client: client.cpp 
	$(CC) client.cpp $(CFLAGS) -o client

server: server.cpp 
	$(CC) server.cpp $(CFLAGS) -o server

clean:
	rm -f client server *.o

