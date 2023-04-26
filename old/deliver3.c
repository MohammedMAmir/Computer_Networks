#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <linux/limits.h>
#include <arpa/inet.h>
#include <time.h>

#define MAXBUFLEN 100

struct packet {
    unsigned int total_frag; // total number of fragments of the file
    unsigned int frag_no; // sequence number of the fragment
    unsigned int size; // size of filedata
    char* filename;
    char filedata[1000];
};


int file_exists(const char *file_name) {
    FILE *file;
    printf("Checking the existence of <%s>\n", file_name);
    if (access(file_name, F_OK) == 0) {
        // file exists
        printf("File exists\n");
        return 1;
    } 
    fprintf(stderr, "file error: %s\n", strerror(errno));
    return 0;
}


long int find_file_size(const char *file_name)
{
    // opening file for read
    FILE *file = fopen(file_name, "r");
  
    fseek(file, 0L, SEEK_SET);
  
    // size of the file
    long int size = ftell(file);
  
    // closing the file
    fclose(file);
  
    return size;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: deliver server_address port_number\n");
    } else {
        struct addrinfo hints, *servinfo;
        char command[MAXBUFLEN], reply[MAXBUFLEN];
        char file_path[PATH_MAX];

        // get ip and port from user

        memset(&hints, 0, sizeof(hints)); // empty the struct
        hints.ai_family = AF_UNSPEC; // only IPv4
        hints.ai_socktype = SOCK_DGRAM;

        if(strlen(argv[1]) <= 5 && strncmp(argv[1], "ug", 2)) {
            strcat(argv[1], ".eecg.utoronto.ca");
        }

        int status = getaddrinfo(argv[1], argv[2], &hints, &servinfo);

        if (status != 0) {
            printf("Hostname: %s, Port: %s\n", argv[1], argv[2]);
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            return status;
        }

        // get input from user

        printf("Input message in the form: ftp <file_name>\n");
        scanf("%s", command);
        scanf("%s", file_path);

        printf("\n");
        if ((strcmp(command, "ftp") == 0)) {
            if (file_exists(file_path)) {
                struct addrinfo *ptr;
                struct sockaddr_storage receiving_addr;
                char ipstr[INET_ADDRSTRLEN];
                void *addr;
                int len, bytes_sent, bytes_received, receiving_len;
                time_t start, end;

                // get IP address
                for (ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
                    if (ptr->ai_family == AF_INET) {
                        struct sockaddr_in *ip = (struct sockaddr_in *)servinfo->ai_addr;
                        addr = &(ip->sin_addr);
                    }

                    // convert the IP to a string and print it
                    inet_ntop(servinfo->ai_family, addr, ipstr, sizeof ipstr);
                    printf("  %s: %s\n", "IPv4", ipstr);
                
                    // create socket
                    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
                    if (sockfd < 0) {
                        fprintf(stderr, "socket error: %s\n", strerror(errno));
                    }

                    // send a message to the server and count time taken
                    time(&start);
                    bytes_sent = sendto(sockfd, command, MAXBUFLEN, 0, ptr->ai_addr, ptr->ai_addrlen);

                    if (bytes_sent < 0) {
                        fprintf(stderr, "sendto error: %s\n", gai_strerror(bytes_sent));
                        return bytes_sent;
                    }

                    // printf("\nclient: waiting for server response\n");
        
                    bytes_received = recvfrom(sockfd, reply, MAXBUFLEN - 1, 0, (struct sockaddr *) &receiving_addr, &receiving_len);
                    if (bytes_received < 0) {
                        fprintf(stderr, "recvfrom error: %s\n", gai_strerror(bytes_received));
                        return bytes_received;
                    }
                    
                    printf("client: got packet from server\n");
                    //printf("client: packet is %d bytes long\n", bytes_received);
                    //printf("client: packet contains \"%s\"\n", reply);
                    close(sockfd);
                }

                if (strncmp(reply, "yes", strlen("yes")) == 0) {
                    printf("\nA file transfer can start.\n");

                    // ---------------------- Section 2 ----------------------

                    // find size of file
                    long int file_size = find_file_size(file_path);

                    // set total number of fragments
                    int frag_num = (file_size / 1000) + 1;

                    printf("\nThe file has %ld fragments\n", frag_num);

                    // open file for reading
                    FILE *file;
                    file = fopen(file_path, "r");

                    int sq_num = 1;
                    
                    // send fragments one by one until nothing left to send
                    while (1) {
                        // create packet
                        struct packet pckt;

                        pckt.filename = file_path;
                        pckt.frag_no = sq_num;
                        pckt.total_frag = frag_num;

                        size_t bytes_read = fread(pckt.filedata, sizeof(pckt.filedata), 1, file);
                        pckt.size = bytes_read * sizeof(*pckt.filedata);
                        printf("\n\n%s\n\n", pckt.filedata);
                        
                        //char pckt[5000];
                        //sprintf(pckt, "%u:%u:%u:%s:%s", msg.total_frag, msg.frag_no, msg.size, msg.filename, msg.filedata);

                        printf("\n------------ PACKET %u ------------\n", pckt.frag_no);
                        //printf("\n\n%s\n\n", pckt);
                        
                        sq_num++;

                        if (bytes_read != 1) {
                            break;
                        }
                       
                        // send packet 
                        // bytes_sent = sendto(sockfd, command, MAXBUFLEN, 0, ptr->ai_addr, ptr->ai_addrlen);

                        // if (bytes_sent < 0) {
                        //     fprintf(stderr, "sendto error: %s\n", gai_strerror(bytes_sent));
                        //     return bytes_sent;
                        // }

                        // sleep for 1 second - temporary
                        sleep(1);
                    }
                
                    printf("\n");

                    // ---------------------- Section 2 END ----------------------
                }

                freeaddrinfo(servinfo);
                time(&end);
                double elapsedTime = difftime(end, start);
                printf("client: time taken is %f ms\n", elapsedTime);
            }
        }
    }

    return 0;
}