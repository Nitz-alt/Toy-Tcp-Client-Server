CC = gcc
FLAGS = -g -O0 -D_POSIX_C_SOURCE=200809 -Wall -std=c11



%.o: %.c
	$(CC) $(FLAGS) $^ -o $*.o

client.o: pcc_client.c server.o
	$(CC) $(FLAGS) pcc_client.c -o client.o

server.o: pcc_server.c
	$(CC) $(FLAGS) pcc_server.c -o server.o





clean:
	rm -f *.o
	rm -f *.out