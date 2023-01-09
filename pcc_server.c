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
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

int EXIT = 0;
uint32_t *pcc_total;
int listenfd, connfd;


void setExit(int  signum){
    EXIT = 1;
}

void print_counts_and_exit(int signum){
    for (int i = 0; i <= 95; i++){
        printf("char '%c' : %u times\n", (char) i + 32, pcc_total[i]);
    }
    free(pcc_total);
    close(listenfd);
    close(connfd);
    exit(0);
}


int read_from_socket(int socket_fd, char * buffer, size_t bytes_num){
    int bytes = 0;
    int totalBytes = 0;
    while(1){
        bytes = read(socket_fd, buffer + totalBytes, bytes_num - totalBytes);
        if (bytes == 0 || (bytes == -1 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))){
            return -1;
        }
        totalBytes += bytes;
        if (totalBytes == bytes_num){
            break;
        }
    }
    return totalBytes;
}

int write_to_socket(int socket_fd, char * buffer, size_t bytes_num){
    int bytes = 0;
    int totalBytes = 0;
    while(1){
        bytes = write(socket_fd, buffer + totalBytes, bytes_num - totalBytes);
        if (bytes == 0 || (bytes == -1 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))){
            return -1;
        }
        totalBytes += bytes;
        if (totalBytes == bytes_num){
            break;
        }
    }
    return totalBytes;
}



int main(int argc, char *argv[])
{
    // Validating correct number of arguments:
    if (argc != 2){
        perror("SERVER: Number of arguments not valid.");
        exit(1);
    }



    struct sockaddr_in serv_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );

    char *data_buff;
    uint16_t port;
    uint32_t file_size;
    ssize_t bytesRead, totalBytesRead;
    char *buffer;
    char charater;
    uint32_t total_printables;
    int reuse;
    uint32_t *pcc_total_atm;
    uint32_t current_bytes_read_count;

    struct sigaction sa_mid, sa_finish;

    port = atoi(argv[1]);
    connfd = -1;

    sa_mid.sa_handler = setExit;
    sigemptyset(&sa_mid.sa_mask);
    sa_finish.sa_handler = print_counts_and_exit;
    sigemptyset(&sa_finish.sa_mask);

    if (sigaction(SIGINT, &sa_finish, NULL) < 0){
        perror("SERVER: Failed at sigaction");
        return 1;
    }

    listenfd = socket( AF_INET, SOCK_STREAM, 0 );

    memset( &serv_addr, 0, addrsize );
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Setting socket so port can be used again
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(int)) < 0){
        perror("SERVER: Error at setting socket");
        close(listenfd);
        exit(1);
    }

    if( 0 != bind(listenfd, (struct sockaddr*) &serv_addr, addrsize ) ){
        perror("SERVER: Error at binding socket");
        close(listenfd);
        exit(1);
    }

    if(0 != listen( listenfd, 10 )){
        perror("SERVER: Error at listening");
        close(listenfd);
        exit(1);
    }


    pcc_total = calloc(sizeof(uint32_t), 95);
    pcc_total_atm = calloc(sizeof(uint32_t), 95);
    data_buff = malloc(ONE_MB);
    while(1){
        // Accept a connection.
        connfd = accept( listenfd, NULL, NULL);

        if( connfd < 0 ){
            perror("SERVER: Error at accepting connection");
            if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                continue;
            }
            close(listenfd);
            exit(1);
        }

        if (sigaction(SIGINT, &sa_mid, NULL) < 0){
            perror("SERVER: Failed at sigaction");
            close(connfd);
            close(listenfd);
            return 1;
        }
        

        // Reading number of bytes
        buffer = (char *) &file_size;
        if (read_from_socket(connfd, buffer, 4) < 0){
            perror("SERVER: Error at reading from client");
            close(connfd);
            continue;
        }
        file_size = ntohl(file_size);

        // Reading file data from client (in batches of 1MB)
        bytesRead = 0;
        totalBytesRead = 0;
        buffer = data_buff;
        total_printables = 0;
        current_bytes_read_count = 0;
        memset(pcc_total_atm, 0, sizeof(uint32_t) * 95);
        while(totalBytesRead < file_size){
            current_bytes_read_count = MIN(ONE_MB, file_size - totalBytesRead);
            bytesRead = read_from_socket(connfd, buffer, current_bytes_read_count);
            if (bytesRead < 0){
                break;
            }
            // Check bytes for printable characters
            for (int i = 0 ; i < bytesRead; i++){
                charater = buffer[i];
                if (IS_PRINTABLE_CHAR(charater)){
                    pcc_total_atm[charater - 32]++;
                    total_printables++;
                }
            }
            totalBytesRead += bytesRead;
        }
        if (bytesRead < 0){
            perror("SERVER: TCP Error occured");
            close(connfd);
            continue;
        }

        // Sending # of printables to the client
        total_printables = htonl(total_printables);
        buffer = (char *) &total_printables;
        if (write_to_socket(connfd, buffer, 4) < 0){
            perror("SERVER: TCP Error occured");
            close(connfd);
            continue;
        }

        // Copying result
        for (int i = 0; i < 95; i++){
            pcc_total[i] += pcc_total_atm[i];
        }

        if (sigaction(SIGINT, &sa_finish, NULL) < 0){
            perror("SERVER: Error at setting signal handler");
            close(connfd);
            close(listenfd);
            return 1;
        }
        if (EXIT){
            print_counts_and_exit(SIGINT);
        }
        close(connfd);
    }
    close(listenfd);
}
