CC = gcc
FLAGS = -g -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11


client.o: pcc_client.c server.o
	$(CC) $(FLAGS) pcc_client.c -o client.o

server.o: pcc_server.c
	$(CC) $(FLAGS) pcc_server.c -o server.o

clean:
	rm *.o