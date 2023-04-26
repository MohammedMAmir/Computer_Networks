#include "packet.h"

#define PACKETBUF 1500

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
    pckt->filedata[pckt->size] = '\0';
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

    //seed packet drop simulation
    srand(time(NULL));
    
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
    unsigned char packet_buffer[PACKETBUF];
    int pktsRec = 0;
    int totalFragNum = 1;
    printf("Waiting for client to send first data packet.\n");\
    FILE* write_file;
    do{
        int dropOrNot = rand()%100;
        if((numbytes = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, 
            (struct sockaddr *)&their_addr, &addr_len)) == -1){
                perror("recvfrom");
                exit(1);
            }
        //if random variable is less than 70, process the received packet, otherwise, drop it.    
        if(dropOrNot < 70){
            // create packet
            struct packet pckt;
            pktsRec++;

            
            // extract data from str received to pckt
            extractData(packet_buffer, &pckt);

            printf("\nReceived packet %d/%d with size of %d bytes\n", pckt.frag_no, pckt.total_frag, numbytes);

            totalFragNum = pckt.total_frag;

            if(pckt.frag_no == 1){
                write_file = fopen(pckt.filename, "wb");
            }
            fwrite(pckt.filedata, sizeof(char), pckt.size, write_file);


            char *message;
            message = malloc(sizeof(char) * 3);
            strcpy(message, "OK");;

            /*Send an acknowledgement to clients' request */
            if ((numbytes = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &their_addr, sizeof(struct sockaddr_storage)))  == -1) {
                fprintf(stderr, "sendto error: %s\n", gai_strerror(numbytes));
                return numbytes;
            }
            
            free(message);
        } else {
            printf("Packet dropped\n");
        }


    } while(pktsRec < totalFragNum);
    
    fclose(write_file);
    close(sockfd);
    
    return 0;
}
