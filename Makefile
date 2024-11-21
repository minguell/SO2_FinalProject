# Parte 1 - Sistemas Operacionais 2 (2024/2) - Weverton Cordeiro
# Grupo: Bruno Alexandre - 00550177, Miguel Dutra - 00342573 e Nathan Mattes - 00342941

CC=gcc
CFLAGS=-I. -lpthread
OBJ = server.o client.o
CLN = server client

all: client server

client: client.c
	$(CC) -c client.c $(CFLAGS)
	$(CC) -o $@ client.o $(CFLAGS)

server: server.c
	$(CC) -c server.c $(CFLAGS)
	$(CC) -o $@ server.o $(CFLAGS)

clean:
	rm *.o $(CLN)

run: 
	@./saida
