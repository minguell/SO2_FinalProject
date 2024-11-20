# Parte 1 - Sistemas Operacionais 2 (2024/2) - Weverton Cordeiro
# Grupo: Bruno Alexandre - 00550177, Miguel Dutra - 00342573 e Nathan Mattes - 00342941

CC=g++
CFLAGS=-I.
OBJ = cpp.o
CLN = saida

all: saida

saida: com.cpp
	$(CC) -c com.cpp $(CFLAGS)
	$(CC) -o $@ $(OBJ) $(CFLAGS)

clean:
	rm *.o $(CLN)

run: 
	@./saida
