#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#define IS_PRINTABLE_CHAR(X) ((32 <= X) && (X <= 126))
#define ONE_MB 0x100000

int EXIT = 0;
uint32_t *pcc_total;


void setExit(int  signum){
    EXIT = 1;
}

void print_counts_and_exit(int signum){
    for (int i = 0; i <= 95; i++){
        printf("char '%c' : %u times\n", (char) i + 32, pcc_total[i]);
    }
    free(pcc_total);
    exit(0);
}



int main(int argc, char *argv[])
{
    // Validating correct number of arguments:
    if (argc != 2){
        fprintf(stderr, "Number of arguments not valid.\n");
        return 1;
    }


    int listenfd  = -1;
    int connfd    = -1;

    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );

    char *data_buff;
    uint16_t port = atoi(argv[1]);
    uint32_t file_size;
    ssize_t bytesRead, totalBytesRead, bytesSent, totalBytesSent;
    char *buffer;
    char charater;
    uint32_t total_printables;
    int reuse;
    struct sigaction sa_mid, sa_finish;
    sa_mid.sa_handler = setExit;
    sigemptyset(&sa_mid.sa_mask);
    sa_finish.sa_handler = print_counts_and_exit;
    sigemptyset(&sa_finish.sa_mask);

    sigaction(SIGINT, &sa_finish, NULL);

    listenfd = socket( AF_INET, SOCK_STREAM, 0 );
    memset( &serv_addr, 0, addrsize );

    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Setting socket so port can be used again
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(int)) < 0){
        // writeError("setsocketopt failed\n");
        perror(NULL);
        close(listenfd);
        return 1;
    }

    if( 0 != bind( listenfd,
                    (struct sockaddr*) &serv_addr,
                    addrsize ) )
    {
        // printf("\n Error : Bind Failed. %s \n", strerror(errno));/
        perror(NULL);
        close(listenfd);
        return 1;
    }

    if( 0 != listen( listenfd, 10 ) )
    {
        // printf("\n Error : Listen Failed. %s \n", strerror(errno));
        perror(NULL);
        close(listenfd);
        return 1;
    }


    pcc_total = calloc(sizeof(uint32_t), 95);
    data_buff = malloc(ONE_MB);
    while(1)
    {
        // Accept a connection.
        connfd = accept( listenfd, NULL, NULL);
        sigaction(SIGINT, &sa_mid, NULL);
        
        if( connfd < 0 )
        {
        // printf("\n Error : Accept Failed. %s \n", strerror(errno));
        perror(NULL);
        close(listenfd);
        return 1;
        }
        // Reading number of bytes
        buffer = (char *) &file_size;
        bytesRead = 0;
        while(1){
            bytesRead += read(connfd, buffer + bytesRead, 4 - bytesRead);
            if (bytesSent < 0){
                perror(NULL);
                close(connfd);
                close(listenfd);
                return 1;
            }
            if (bytesRead == 4){
                break;
            }
        }
        file_size = ntohl(file_size);

        bytesRead = 0;
        totalBytesRead = 0;
        buffer = data_buff;
        total_printables = 0;
        while(1){
            bytesRead = read(connfd, buffer, file_size - totalBytesRead);
            if (bytesRead < 0){
                // fprintf(stderr, "Connection was close\n");
                perror(NULL);
                close(connfd);
                close(listenfd);
                return 1;
            }
            // Check bytes for printable characters
            for (int i = 0 ; i < bytesRead; i++){
                charater = buffer[i];
                if (IS_PRINTABLE_CHAR(charater)){
                    pcc_total[charater - 32]++;
                    total_printables++;
                }
            }
            totalBytesRead += bytesRead;
            if (totalBytesRead == file_size){
                break;
            }
        }

        bytesSent = 0;
        totalBytesSent = 0;
        total_printables = htonl(total_printables);
        buffer = (char *) &total_printables;
        while (1){
            bytesSent = write(connfd, buffer + totalBytesSent, 4 - bytesSent);
            if (bytesSent < 0){
                // fprintf(stderr, "Error at writing to socket\n");
                perror(NULL);
                close(connfd);
                close(listenfd);
                return 1;
            }
            totalBytesSent += bytesSent;
            if (totalBytesSent == 4){
                break;
            }
        }
        // close socket
        close(connfd);
        sigaction(SIGINT, &sa_finish, NULL);
        if (EXIT){
            print_counts_and_exit(SIGINT);
        }
    }
    close(listenfd);
}
