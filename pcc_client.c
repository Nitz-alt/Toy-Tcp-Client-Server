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

typedef ssize_t (*socket_op) (int, void *, size_t);


void writeError(char *msg){
    fprintf(stderr, msg, NULL);
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
    return 0;
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
    return 0;
}




int main(int argc, char *argv[])
{
    if (argc != 4){
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }
    int  sockfd     = -1;
    uint16_t port;
    char *file_path, *file_memory;
    int fd;
    struct stat file_stat;
    char *buffer;
    uint32_t fileSize, printable_char;
    // int reuse;
    struct sockaddr_in serv_addr; // where we Want to get to
    char *ip_addr = argv[1];


    port = (uint16_t) atoi(argv[2]);
    file_path = argv[3];
    if (access(file_path, F_OK | R_OK) < 0){
        perror(NULL);
        return 1;
    }

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Creating socket failed\n");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); // Note: htons for endiannes
    if (inet_pton(AF_INET, ip_addr, &(serv_addr.sin_addr.s_addr)) < 0){
        perror("Failed at inet_pton\n");
        close(sockfd);
        return 1;
    }


    // connect socket to the target address
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){   
        perror("Connecting failed\n");
        close(sockfd);
        return 1;
    }
    // Opening file to read
    fd = open(file_path, O_RDONLY);
    if (fd < 0){
        perror("Erorr opening file\n");
        return 1;
    }

    // Getting file size
    if (fstat(fd, &file_stat) < 0){
        perror("Error at fstat\n");
        close(sockfd);
        close(fd);
        return 1;
    }

    // Sending file size
    fileSize = htonl(file_stat.st_size);
    buffer = (char *) &fileSize;
    if (write_to_socket(sockfd, buffer, 4) < 0){
        perror("Failed at sending data to server\n");
        return 1;
    }

    // Sending file data
    file_memory = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (write_to_socket(sockfd, file_memory, file_stat.st_size) < 0){
        perror("Failed at sending data to server\n");
        munmap(file_memory, file_stat.st_size);
        close(sockfd);
        close(fd);
        return 1;
    }

    // Getting # of printable characters
    buffer = (char *) &printable_char;
    if (read_from_socket(sockfd, buffer, 4) < 0){
        perror("Error reading data from server\n");
        munmap(file_memory, file_stat.st_size);
        close(sockfd);
        close(fd);
        return 1;
    }
    printable_char = ntohl(printable_char);

    // Printing # number of printable chars
    printf("# of printable characters: %u\n", printable_char);
    
    munmap(file_memory, file_stat.st_size);
    close(fd);
    close(sockfd);
    return 0;
}
