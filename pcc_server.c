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

#define IS_PRINTABLE_CHAR(X) ((32 <= (X)) && ((X) <= 126))
#define ONE_MB 0x100000
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
typedef ssize_t (*socketopt)(int, const void *, size_t);


int EXIT = 0;
uint32_t pcc_total[95];
int listenfd, connfd;
struct sockaddr_in peer_addr;

void print_counts_and_exit(){
    for (int i = 0; i <= 95; i++){
        printf("char '%c' : %u times\n", (char) i + 32, pcc_total[i]);
    }
    close(listenfd);
    exit(0);
}

void sig_handle(int  signum){
    EXIT = 1;
    if (ntohl(peer_addr.sin_addr.s_addr) == 0){
        // There is not connection currently ==> print and die
        print_counts_and_exit();
    }
}

int socket_opt(int socket_fd, char * buffer, size_t bytes_num, socketopt opt){
    int bytes = 0;
    int totalBytes = 0;
    while(1){
        bytes = (*opt)(socket_fd, buffer + totalBytes, bytes_num - totalBytes);
        // Our tcp error instruction
        if ((bytes == 0) || (bytes == -1 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))){
            return 0;
        }
        else if (bytes == -1){
            return -1;
        }
        totalBytes += bytes;
        if (totalBytes == bytes_num){
            break;
        }
    }
    return totalBytes;
}

int read_from_socket(int socket_fd, char * buffer, size_t bytes_num){               
    return socket_opt(socket_fd, buffer, bytes_num, (ssize_t (*)(int, const void *, size_t)) &read);
}

int write_to_socket(int socket_fd, char * buffer, size_t bytes_num){
    return socket_opt(socket_fd, buffer, bytes_num, &write);
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

    char data_buff[ONE_MB];
    uint16_t port;
    uint32_t file_size;
    ssize_t bytesRead, totalBytesRead;
    char *buffer;
    char charater;
    uint32_t total_printables;
    int reuse = 1;
    int result;
    uint32_t pcc_total_atm[95];
    uint32_t current_bytes_read_count;

    struct sigaction sa;

    port = atoi(argv[1]);
    connfd = -1;

    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sig_handle;

    if (sigaction(SIGINT, &sa, NULL) < 0){
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

    memset(pcc_total, 0, sizeof(uint32_t) * 95);
    memset(pcc_total_atm, 0, sizeof(uint32_t) * 95);
    memset(data_buff, 0, ONE_MB);
    while(1){
        while(1){
            // Accept a connection.
            connfd = accept(listenfd, (struct sockaddr *) &peer_addr, &addrsize);
            if( connfd < 0 ){
                perror("SERVER: Error at accepting connection");
                if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                    break;
                }
                close(connfd);
                exit(EXIT_FAILURE);
            }
            // Reading number of bytes
            buffer = (char *) &file_size;
            result = read_from_socket(connfd, buffer, 4);
            if (result == 0){
                perror("SERVER: TCP Error occured, read size");
                close(connfd);
                break;
            }
            else if (result == -1){
                exit(EXIT_FAILURE);
            }
            file_size = ntohl(file_size);

            // Reading file data from client (in batches of 1MB)
            bytesRead = 0;
            totalBytesRead = 0;
            buffer = data_buff;
            total_printables = 0;
            current_bytes_read_count = 0;
            
            while(totalBytesRead < file_size){
                current_bytes_read_count = MIN(ONE_MB, file_size - totalBytesRead);
                bytesRead = read_from_socket(connfd, buffer, current_bytes_read_count);
                if (bytesRead <= 0){
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
            if (bytesRead == 0){
                perror("SERVER: TCP Error occured, read file");
                close(connfd);
                break;
            }
            if (bytesRead == -1){
                exit(EXIT_FAILURE);
            }

            // Sending # of printables to the client
            total_printables = htonl(total_printables);
            buffer = (char *) &total_printables;
            result = write_to_socket(connfd, buffer, 4);
            if ( result == 0){
                perror("SERVER: TCP Error occured, write printables count");
                close(connfd);
                break;
            }
            else if (result == -1){exit(EXIT_FAILURE);}

            if (close(connfd) < -1){
                break;
            }
            
            // Copying result
            for (int i = 0; i < 95; i++){
                pcc_total[i] += pcc_total_atm[i];
            }
            break;
        }
        if (EXIT == 1){
            print_counts_and_exit();
        }
        memset(&peer_addr, 0, addrsize);
        memset(pcc_total_atm, 0, sizeof(uint32_t) * 95);
    }
}
