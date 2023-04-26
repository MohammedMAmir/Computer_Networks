#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <unistd.h> // for close

#define MAXBUFLEN 2000

struct packet {
    unsigned int total_frag; // total number of fragments of the file
    unsigned int frag_no; // sequence number of the fragment
    unsigned int size; // size of filedata
    char* filename;
    unsigned char filedata[1000];
};

void extractData(unsigned char* pckt_str, struct packet* pckt) {
    int total_frag_end;
    int frag_no_end;
    int size_end;
    int name_end;

    char filedata[1000];

    // find total_frag
    for(int i = 0; i < MAXBUFLEN; i++){
        if(pckt_str[i] == ':'){
            total_frag_end = i;
            break;
        }
    }
    char total_frag_str[total_frag_end];
    for(int i = 0; i < total_frag_end; i++){
        total_frag_str[i] = pckt_str[i];
    }
    pckt->total_frag = atoi(total_frag_str);

    //find frag_no
    for(int i = total_frag_end + 1; i < MAXBUFLEN; i++){
        if(pckt_str[i] == ':'){
            frag_no_end = i;
            break;
        }
    }
    char frag_no_str[frag_no_end - total_frag_end - 1];
    for(int i = 0; i < frag_no_end - total_frag_end - 1; i++){
        frag_no_str[i] = pckt_str[i + total_frag_end + 1];
    }
    pckt->frag_no = atoi(frag_no_str);
    
    //find size
    for(int i = frag_no_end + 1; i < MAXBUFLEN; i++){
        if(pckt_str[i] == ':'){
            size_end = i;
            break;
        }
    }
    char size_str[size_end - frag_no_end  - 1];
    for(int i = 0; i < size_end - frag_no_end  - 1; i++){
        size_str[i] = pckt_str[i + frag_no_end + 1];
    }
    pckt->size = atoi(size_str);

    //find file_name
    for(int i = size_end + 1; i < MAXBUFLEN; i++){
        if(pckt_str[i] == ':'){
            name_end = i;
            break;
        }
    }
    pckt->filename = (char*)malloc((name_end - size_end - 1) * sizeof(char));
    for(int i = 0; i < name_end - size_end - 1; i++){
        pckt->filename[i] = pckt_str[i + size_end + 1];
    }

    //copy file data, character by character
    for (int i = 0; i < 1000; i++) {
        filedata[i] = pckt_str[name_end + 1 + i];
    }
    for (int i = 0; i < pckt->size; i++) {
        pckt->filedata[i] = filedata[i];
    }
    printf("\n\n------------------------------\nDATA: %s\n", pckt->filedata);
    //for(int i = name_end + 1; i < MAXBUFLEN; i++){
    //    pckt->filedata[i - (name_end + 1)] = pckt_str[i];
    //}
    //memcpy(pckt->filedata, &pckt_str[name_end + 1], 1000);
}

// get sockaddr, IPv4 or IPv6:
 void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
         return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/*
 * 
 */
int main(int argc, char** argv) {
    char* myPort = argv[1];
    int sockfd;
    struct addrinfo hints, *servinfo, *p, *their_socket;
    int rv;
    int numbytes;
    
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; //set hints to use AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM; //use dgram UDP socket
    hints.ai_flags = AI_PASSIVE; //use my IP
    
    //error checking for getaddrinfo
    //getaddreinfo returns list of socket addressinfo structures that can be 
    //connected to based on criteria in hints
    if((rv = getaddrinfo(NULL, myPort, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
    
    //loop through all possible addresses and bind to the first one we can
    for(p = servinfo; p != NULL; p=p->ai_next){
        //create file descriptor for socket based on addressinfo structure we chose
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("listener: socket");
            continue;
        }
        
        //bind the file descriptor to the socket address
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        
        break;
    }
    
    if(p == NULL){
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    
    freeaddrinfo(servinfo);
    
    printf("listener: waiting for recvfrom...\n");
    

    //* Wait to recieve request from a client *//
    addr_len = sizeof their_addr;
    if((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, 
            (struct sockaddr *)&their_addr, &addr_len)) == -1){
        perror("recvfrom");
        exit(1);
    }

    their_socket = (struct sockaddr_in *) &their_addr;

    printf("listener: got packet from %s\n", inet_ntop(their_socket->ai_addr,
        get_in_addr((struct sockaddr*)&their_addr), s, sizeof s));
    //printf("listener: packet is %d bytes long\n", numbytes);
    //printf("listener: packet contains \"%s\"\n", buf);


    char *message;
    if (strcmp(buf, "ftp")) {
        message = malloc(sizeof(char) * 3);
        strcpy(message, "no");
    } else {
        message = malloc(sizeof(char) * 4);
        strcpy(message, "yes");
    }

    printf("Size: %d\n\n", addr_len);

    /*Send an acknowledgement to clients' request */
    if ((numbytes = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &their_addr, sizeof(struct sockaddr_storage)))  == -1) {
        fprintf(stderr, "sendto error: %s\n", gai_strerror(numbytes));
        return numbytes;
    }
    
    free(message);

    /*  Wait for client to start sending packet data */
    unsigned char packet_buffer[MAXBUFLEN];
    int pktsRec = 0;
    int totalFragNum = 0;
    printf("Waiting for client to send first data packet.\n");\
    FILE* write_file;
    do{
        if((numbytes = recvfrom(sockfd, packet_buffer, MAXBUFLEN - 1, 0, 
            (struct sockaddr *)&their_addr, &addr_len)) == -1){
                perror("recvfrom");
                exit(1);
            }
        packet_buffer[numbytes] = '\0';

        // create packet
        struct packet pckt = {.filedata = {0}, .filename = NULL, .frag_no = -1, .size = -1, .total_frag = -1};
        pktsRec++;

        printf("\nBUFFER: %s",packet_buffer);
        
        // extract data from str received to pckt
        extractData(packet_buffer, &pckt);

        totalFragNum = pckt.total_frag;

        printf("\n\nlistener: fragment %d/%d \n", pckt.frag_no, totalFragNum);
        //printf("listener: packet contains \"%s\"\n", pckt.filedata);

        // if(pckt.frag_no == 1){
        //     write_file = fopen("server_testfile.txt", "wb");
        //     //fputs(pckt.filedata, write_file);
        //     //fclose(write_file);
        // } else{
        //     fopen("server_testfile.txt", "ab");
        //     fputs(pckt.filedata, write_file);
        //     fclose(write_file);
        //  }

        if (pckt.frag_no == 1) {
            write_file = fopen("server_testfile.txt", "wb");
        }

        // write to file
        fwrite(pckt.filedata, 1, pckt.size, write_file);

        char *message;
        message = malloc(sizeof(char) * 3);
        strcpy(message, "OK");;

         /*Send an acknowledgement to clients' request */
        if ((numbytes = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &their_addr, sizeof(struct sockaddr_storage)))  == -1) {
            fprintf(stderr, "sendto error: %s\n", gai_strerror(numbytes));
            return numbytes;
        }
        
        free(message);
        //free(pckt.filename);
        //free(pckt.filedata);

    } while(totalFragNum != pktsRec);
    
    fclose(write_file);
    close(sockfd);
    
    return 0;
}
