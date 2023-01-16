#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>

#define ONE_MB 0x100000
typedef ssize_t (*socketopt)(int, const void *, size_t);


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
    int  sockfd;
    uint16_t port;
    char *file_path, file_buffer[ONE_MB];
    int fd;
    struct stat file_stat;
    char *buffer;
    uint32_t fileSize, printable_char;
    struct sockaddr_in serv_addr; // where we Want to get to
    char *ip_addr;
    int reuse = 1;
    int totalBytes_sent;
    int bytes_sent;
    int result;
    int current_bytes_num_read;

    if (argc != 4){
        perror("CLIENT: Invalid arguments. Usage: ./client <ip address> <port> <file name>\nErrno message");
        return 1;
    }
    ip_addr = argv[1];
    port = (uint16_t) atoi(argv[2]);
    file_path = argv[3];
    if (access(file_path, F_OK | R_OK) < 0){
        perror("CLIENT: No permissions to read file ");
        return 0;
    }

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("CLIENT: Creating socket failed ");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); // Note: htons for endiannes
    if (inet_pton(AF_INET, ip_addr, &(serv_addr.sin_addr.s_addr)) < 1){
        perror("CLIENT: Failed at inet_pton ");
        close(sockfd);
        return 1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(int)) < 0){
        perror("CLIENT: Error at setting socket");
        close(sockfd);
        return 1;
    }

    // connect socket to the target address
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){   
        perror("CLIENT: Connecting failed ");
        close(sockfd);
        return 1;
    }

    // Opening file to read
    fd = open(file_path, O_RDONLY);
    if (fd < 0){
        perror("CLIENT: Erorr opening file ");
        return 1;
    }

    // Getting file size
    if (fstat(fd, &file_stat) < 0){
        perror("CLIENT: Error at fstat ");
        close(sockfd);
        close(fd);
        return 1;
    }

    // Sending file size
    fileSize = htonl(file_stat.st_size);
    buffer = (char *) &fileSize;
    result = write_to_socket(sockfd, buffer, 4);
    if ( result == 0){
        perror("CLIENT: Failed at sending data to server ");
        return 1;
    }
    else if (result == -1){exit(EXIT_FAILURE);}

    // Sending file data
    totalBytes_sent = 0;
    bytes_sent = 1;
    current_bytes_num_read = 0;
    while (totalBytes_sent < file_stat.st_size){

        current_bytes_num_read = read(fd, file_buffer, ONE_MB);
        bytes_sent = write_to_socket(sockfd, file_buffer, current_bytes_num_read);
        if (bytes_sent <= 0){
            break;
        }
        assert(current_bytes_num_read == bytes_sent);
        totalBytes_sent += bytes_sent;
    }
    if (bytes_sent == 0){
        perror("CLIENT: TCP Error occured");
        close(sockfd);
        close(fd);
        return 1;
    }
    else if (bytes_sent == -1){exit(EXIT_FAILURE);}

    // Getting # of printable characters
    buffer = (char *) &printable_char;
    result = read_from_socket(sockfd, buffer, 4);
    if ( result == 0){
        perror("CLIENT: TCP Error occured");
        close(sockfd);
        close(fd);
        return 1;
    }
    else if (result == -1){exit(EXIT_FAILURE);}
    
    printable_char = ntohl(printable_char);

    // Printing # number of printable chars
    printf("# of printable characters: %u\n", printable_char);
    
    close(fd);
    close(sockfd);
    return 0;
}
