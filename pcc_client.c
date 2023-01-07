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

void writeError(char *msg){
    fprintf(stderr, msg, NULL);
}


int main(int argc, char *argv[])
{
    int  sockfd     = -1;
    int  bytesSent =  0, totalBytesSent;
    uint16_t port;
    char *file_path, *file_memory;
    int fd;
    struct stat file_stat;

    struct sockaddr_in serv_addr; // where we Want to get to
    socklen_t addrsize = sizeof(struct sockaddr_in );

    port = (uint16_t) atoi(argv[2]);
    file_path = argv[2];
    if (access(file_path, F_OK | R_OK) < 0){
        fprintf(stderr, "File does not exists or not readable\n");
        return 1;
    }

    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "\n Error : Could not create socket \n");
        return 1;
    }


    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); // Note: htons for endiannes
    inet_pton(AF_INET, argv[1], &(serv_addr.sin_addr.s_addr)); // hardcoded...

    // connect socket to the target address
    if( connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "\n Error : Connect Failed. %s \n", strerror(errno));
        return 1;
    }

    fd = open(file_path, O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "Failed at opening file\n");
    }
    // Opening file to read
    if (fstat(fd, &file_stat) < 0){
        fprintf(stderr, "Error getting size of file\n");
    }
    file_memory = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    while(1)
    {
        bytesSent = write(sockfd, file_memory + totalBytesSent, file_stat.st_size - totalBytesSent);
        if (bytesSent < 0){
            fprintf(stderr, "Error sending data\n");
            return 1;
        }
        totalBytesSent += bytesSent;
        if (totalBytesSent == file_stat.st_size){
            break;
        }
    }


    close(sockfd);
    return 0;
}
