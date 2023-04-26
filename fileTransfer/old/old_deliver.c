#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <linux/limits.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>

#define MAXBUFLEN 2000

struct packet {
    unsigned int total_frag; // total number of fragments of the file
    unsigned int frag_no; // sequence number of the fragment
    unsigned int size; // size of filedata
    char* filename;
    unsigned char filedata[1000];
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


long int find_file_size(const char file_name[])
{
    // opening file for read
    FILE *file = fopen(file_name, "r");
  
    fseek(file, 0L, SEEK_END);
  
    // size of the file
    long int size = ftell(file);
  
    // closing the file
    fclose(file);
  
    return size;
}

int getIntLength(unsigned int x) {
    int count = 0;
    while(x != 0) {
        x /= 10;
        count++;
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: deliver server_address port_number\n");
    } else {
        struct addrinfo hints, *servinfo;
        char command[MAXBUFLEN], reply[MAXBUFLEN];
        char file_path[PATH_MAX] = {0};

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
                    clock_t start, end;  // timer variables
                    start = clock();
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

                    end = clock();
                    fprintf(stdout, "\nRTT = %f sec.\n", ((double) (end - start) / CLOCKS_PER_SEC));

                    if (strncmp(reply, "yes", strlen("yes")) == 0) {
                        printf("\nA file transfer can start.\n");

                        // ---------------------- Section 2 ----------------------

                        // find size of file
                        long int file_size = find_file_size(file_path);

                        // set total number of fragments
                        int frag_num = (file_size / 1000) + 1;

                        printf("\nThe file has %d fragments\n\n", frag_num);

                        // open file for reading
                        FILE *file;
                        size_t bytes_read = 0;
                        unsigned char *read_buf = (unsigned char *)malloc((file_size + 1) * sizeof(unsigned char));

                        printf("File name: %s\n", file_path);
                        file = fopen(file_path, "rb");
                    
                        int sq_num = 1;
                        
                        //printf("\nThe file has size %d\n\n", file_size);
                        //printf("\nDATA: %s\n\n", read_buf);

                        // send fragments one by one until nothing left to send
                        while ((bytes_read = fread(read_buf, 1, 1000, file)) > 0) {
                            // create packet
                            struct packet pckt = {.filedata = {'\0'}, .filename = NULL, .frag_no = sq_num, .size = bytes_read, .total_frag = frag_num};

                            printf("\nRead %d bytes\n\n", bytes_read);

                            // Update filename and increase sequence number
                            pckt.filename = strdup(file_path);
                            sq_num++;

                            //memcpy(pckt.filedata, read_buf, sizeof(pckt.filedata));

                            // Turn packet into string
                            int total_frag_length = getIntLength(pckt.total_frag);
                            int frag_no_length = getIntLength(pckt.frag_no);
                            int size_length = getIntLength(pckt.size);
                            int filename_length = strlen(pckt.filename);
                            int packet_length = total_frag_length + frag_no_length + size_length + filename_length + pckt.size + 4;
                            
                            unsigned char *pckt_str = (unsigned char*)malloc(sizeof(unsigned char) * packet_length);

                            printf("total_frag_length: %d\n", total_frag_length);
                            printf("frag_no_length: %d\n", frag_no_length);
                            printf("size_length: %d\n", size_length);
                            printf("Filename length: %d\n", filename_length);
                            printf("Packet length: %d\n", packet_length);


                            // Copy file data into packet.filedata
                            for (int i = 0; i < pckt.size; i++) {
                                pckt.filedata[i] = read_buf[((pckt.frag_no - 1) * pckt.size) + i];
                            }
                            pckt.filedata[pckt.size] = '\0';

                            printf("\npckt.filedata: %s\n\n", pckt.filedata);

                            // Add total number of fragments
                            char total_frag_str[total_frag_length];
                            sprintf(total_frag_str, "%d:", pckt.total_frag);
                            for(int i = 0; i < total_frag_length + 1; i++){
                                *(pckt_str + i) = total_frag_str[i];
                            }

                            // Add index of current fragment
                            char frag_no_str[frag_no_length];
                            sprintf(frag_no_str, "%d:", pckt.frag_no);
                            for(int i = 0; i < frag_no_length + 1; i++){
                                *(pckt_str + total_frag_length + 1 + i) = frag_no_str[i];
                            }

                            // Add size of current packet
                            char size_str[size_length];
                            sprintf(size_str, "%d:", pckt.size);
                            for(int i = 0; i < size_length + 1; i++){
                                *(pckt_str + total_frag_length + 1 + frag_no_length + 1 + i) = size_str[i];
                            }

                            // Add file name
                            for(int i = 0; i < filename_length; i++){
                                *(pckt_str + total_frag_length + 1 + frag_no_length + 1 + size_length + 1 + i) = pckt.filename[i];
                            }
                            *(pckt_str + total_frag_length + 1 + frag_no_length + 1 + size_length + 1 + filename_length) = ':';

                            // Add packet data
                            for(int i = 0; i < pckt.size; i++){
                                *(pckt_str + total_frag_length + 1 + frag_no_length + 1 + size_length + 1 + filename_length + 1 + i) = pckt.filedata[i];
                            }

                            printf("PacketStr: %s\n\n", pckt.filedata);

                            // send packet 
                            bytes_sent = sendto(sockfd, pckt_str, packet_length, 0, ptr->ai_addr, ptr->ai_addrlen);

                            if (bytes_sent < 0) {
                                fprintf(stderr, "sendto error: %s\n", gai_strerror(bytes_sent));
                                return bytes_sent;
                            }
                            free(pckt.filename);
                            free(pckt_str);
                            
                            // wait for acknoledgement before moving on
                            bytes_received = recvfrom(sockfd, reply, MAXBUFLEN - 1, 0, (struct sockaddr *) &receiving_addr, &receiving_len);
                            if (bytes_received < 0) {
                                fprintf(stderr, "recvfrom error: %s\n", gai_strerror(bytes_received));
                                return bytes_received;
                            }

                            printf("\nAcknoledgement: <%s>\n", reply);
                        }
                    }
                
                    printf("\n");

                    // ---------------------- Section 2 END ----------------------
                    close(sockfd);
                }

                freeaddrinfo(servinfo);
            }
        }
    }

    return 0;
}