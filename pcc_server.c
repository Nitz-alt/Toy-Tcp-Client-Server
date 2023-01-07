#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#define IS_PRINTABLE_CHAR(X) ((32 <= X) && (X <= 126))
#define ONE_MB 0x100000


void writeError(char *msg){
    fprintf(stderr, msg);
}



int main(int argc, char *argv[])
{
    // Validating correct number of arguments:
    if (argc != 2){
        writeError("Number of arguments not valid.\n");
    }


    int totalsent = -1;
    int nsent     = -1;
    int len       = -1;
    int n         =  0;
    int listenfd  = -1;
    int connfd    = -1;

    struct sockaddr_in serv_addr;
    struct sockaddr_in my_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );

    char *data_buff;
    uint32_t *pcc_total;
    uint16_t port = atoi(argv[1]);
    uint32_t file_size;
    ssize_t bytesRead, totalBytesRead, bytesSent;
    char *buffer;
    char charater;
    uint32_t total_printables, total_printables_toNetwork;


    listenfd = socket( AF_INET, SOCK_STREAM, 0 );
    memset( &serv_addr, 0, addrsize );

    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    // Setting socket so port can be used again
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, NULL, NULL);

    if( 0 != bind( listenfd,
                    (struct sockaddr*) &serv_addr,
                    addrsize ) )
    {
        printf("\n Error : Bind Failed. %s \n", strerror(errno));
        return 1;
    }

    if( 0 != listen( listenfd, 10 ) )
    {
        printf("\n Error : Listen Failed. %s \n", strerror(errno));
        return 1;
    }


    pcc_total = calloc(sizeof(uint32_t), 95);
    data_buff = malloc(ONE_MB);
    while( 1 )
    {
        // Accept a connection.
        // Can use NULL in 2nd and 3rd arguments
        // but we want to print the client socket details
        connfd = accept( listenfd,
                        (struct sockaddr*) &peer_addr,
                        &addrsize);

        if( connfd < 0 )
        {
        printf("\n Error : Accept Failed. %s \n", strerror(errno));
        return 1;
        }

        // Reading number of bytes
        buffer = (char *) &file_size;
        bytesRead = 0;
        while(1){
            bytesRead += read(connfd, buffer + bytesRead, 4 - bytesRead);
            if (bytesRead == 4){
                break;
            }
        }

        file_size = ntohl(file_size);
        bytesRead = 0;
        totalBytesRead = 0;
        buffer = data_buff;
        while(1){
            bytesRead = read(connfd, buffer, file_size - totalBytesRead);
            // Check bytes for printable characters
            for (int i = 0 ; i < bytesRead; i++){
                charater = buffer[i];
                if (IS_PRINTABLE_CHAR(charater)){
                    pcc_total[charater - 95]++;
                    total_printables++;
                }
            }
            totalBytesRead += bytesRead;
            if (totalBytesRead == file_size){
                break;
            }
        }

        bytesSent = 0;
        total_printables_toNetwork = htonl(total_printables);
        buffer = (char *) &total_printables_toNetwork;
        while (1){
            bytesSent += write(connfd, buffer + bytesRead, total_printables - bytesSent);
            if (bytesSent == total_printables){
                break;
            }
        }

        // close socket
        close(connfd);
    }
}
