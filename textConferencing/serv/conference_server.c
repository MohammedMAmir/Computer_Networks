#include "packet.h"
#define MAXBUFLEN 4096
#define BACKLOGSIZE 4096
#define MAX_CLIENT 500
#define MAX_SESH 500
#define TIMEOUT 5
#define MAXLINELENGTH 45

struct client* client_list[MAX_CLIENT];
struct session* sesh_list[MAX_SESH];

//looks through the database and finds the key of a user (super slow I know, but easier than a hash function rn) 
int source_key_search(char* source){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(client_list[i] != NULL){
            if(strcmp(client_list[i]->ID, source) == 0){
                return i;
            }
        }
    }
    return -1;
}



int sock_key_search(int sockfd){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(client_list[i] != NULL){
            if(client_list[i]->client_sock == sockfd){
                return i;
            }
        }
    }
    return -1;
}
//find an unused key
int empty_key_search(int client_sesh){
    //if client_sesh is 0, find empty client key
    if(client_sesh == 0){
        for(int i = 0; i < MAX_CLIENT; i++){
            if(client_list[i] == NULL){
                return i;
            }
        }
    //otherwise find an empty session key
    }else{
        for(int i = 0; i < MAX_SESH; i++){
            if(sesh_list[i] == NULL){
                return i;
            }
        }
    }
    return -1;
}

int find_sesh_key(char* session_id){
    for(int i = 0; i < MAX_SESH; i++){
        if(sesh_list[i] != NULL){
            if(strcmp(sesh_list[i]->session_id, session_id) == 0){
            return i;
        }
        }
        
    }
    return -1;
}

void insert_sock_sesh(int session_key, int sockfd, int key){
    if(sesh_list[session_key]->head == NULL){
        sesh_list[session_key]->head = (struct session_part *) malloc(sizeof(struct session_part));
        sesh_list[session_key]->head->client_key = key;
        sesh_list[session_key]->head->client_sock = sockfd;
        sesh_list[session_key]->head->next_part = NULL;
    }else{
        struct session_part* p = sesh_list[session_key]->head;
        while(p->next_part != NULL){
            p = p->next_part;
        }
        p->next_part = (struct session_part *) malloc(sizeof(struct session_part));
        p->next_part->client_key = key;
        p->next_part->client_sock = sockfd;
        p->next_part->next_part = NULL;
        p = NULL;
    }
    return;
}

void delete_sock_sesh(int session_key, int sockfd, int key){
    struct session_part* prev = NULL;
    struct session_part* current = sesh_list[session_key]->head;

    while(current != NULL){
        if(current->client_key == key){
            if(current == sesh_list[session_key]->head){
                sesh_list[session_key]->head = current->next_part;
                free(current);
                current = NULL;
                return;
            }else{
                prev->next_part = current->next_part;
                free(current); 
                current = NULL;
                return;
            }
        }else{
            prev = current;
            current = current->next_part;
        }
    }
}

int join_sesh(int sockfd, char* source, char* session_id){
   int key = sock_key_search(sockfd); 
    if(client_list[key]->in_session == 1){
        return IN_SESSION;
    }else{
        int i = find_sesh_key(session_id);
        if(i != -1){
            insert_sock_sesh(i, sockfd, key);
            sesh_list[i]->participants += 1;
            client_list[key]->in_session = 1;
            client_list[key]->session_id = strdup(session_id); // changed strdup(source) to strdup(session_id)
            return 0;
        }
        return NO_SESH;
    }
}

int create_sesh(int sockfd, char* source, char* session_id){
    int ret = empty_key_search(1);
    if(ret == -1){
        return SESH_MAXED;
    }
    sesh_list[ret] = (struct session *)malloc(sizeof(struct session));
    sesh_list[ret]->session_key = ret;
    sesh_list[ret]->session_id = strdup(session_id);
    sesh_list[ret]->participants = 0;
    sesh_list[ret]->head = NULL;
    return 0;
}

void leave_sesh(int sockfd, char* source){
    int key = sock_key_search(sockfd);
    if(client_list[key]->in_session == 1){
        int sesh_key = find_sesh_key(client_list[key]->session_id);
        delete_sock_sesh(sesh_key, sockfd, key);
        sesh_list[sesh_key]->participants -= 1;
        if(sesh_list[sesh_key]->participants == 0){
            free(sesh_list[sesh_key]);
        }
        client_list[key]->in_session = 0;
        free(client_list[key]->session_id);
    }
    return;
}

int parse_request(int req_sock, struct message* msg){
    if(msg->type == NEW_SESS){
        struct message response;
        int ret = create_sesh(req_sock, msg->source, msg->data);
        if(ret == SESH_MAXED){
            response.type = NS_NAK;
            strcpy(response.source, "server");
            strcpy(response.data, "Server overloaded. Session not created");
            response.size = sizeof("Server overloaded. Session not created");
        }else{
            ret = join_sesh(req_sock, msg->source, msg->data);
            if(ret == IN_SESSION){
                response.type = NS_NAK;
                strcpy(response.source, "server");
                strcpy(response.data, "Client is already in session");
                response.size = sizeof("Client is already in session");
            }else{
                response.type = NS_ACK;
                strcpy(response.source, "server");
                strcpy(response.data, "\0");
                response.size = 1;
            }     
        }
        if(send(req_sock, &response, sizeof(response), 0) < 0){
            printf("Can't send\n");
            return -1;
        }
        return 0;   
    }else if(msg->type == JOIN){
        struct message response;
        int ret = join_sesh(req_sock, msg->source, msg->data);
        if(ret == IN_SESSION){
            response.type = JN_NAK;
            strcpy(response.source, "server");
            strcpy(response.data, "Client is already in session");
            response.size = sizeof("Client is already in session");
        }else if(ret == NO_SESH){
            response.type = JN_NAK;
            strcpy(response.source, "server");
            strcpy(response.data, "Session does not exist");
            response.size = sizeof("Session does not exist");
        }else{
            response.type = JN_ACK;
            strcpy(response.source, "server");
            strcpy(response.data, "\0");
            response.size = 1;
        }
        if(send(req_sock, &response, sizeof(response), 0) < 0){
            printf("Can't send\n");
            return -1;
        }
        return 0;  
    }else if(msg->type == LEAVE_SESS){
        int key = sock_key_search(req_sock);
        if(client_list[key]->in_session == 1){
            leave_sesh(req_sock, msg->source);
        }
    }else if(msg->type == QUERY){
        struct message response;
        strcpy(response.source, "server");
        response.type = QU_ACK;
        sprintf(response.data, "Clients Online: ");
        response.size = sizeof("Clients Online: ");
        int first = 0;
        for(int i = 0; i < MAX_CLIENT; i++){
            if(client_list[i] != NULL){
                if(client_list[i]->logged_in == 1){
                    if(first != 0){
                        sprintf(response.data + strlen(response.data), ", ");
                        response.size += sizeof(", ");
                    }
                    first = 1;
                    sprintf(response.data + strlen(response.data), client_list[i]->ID);          
                    response.size += sizeof(client_list[i]->ID);
                }
            }
        }
        sprintf(response.data + strlen(response.data), "\n");
        response.size += sizeof("\n");
        first = 0;
        sprintf(response.data + strlen(response.data), "Available Sessions: ");
        response.size = sizeof("Available Sessions: ");
        for(int i = 0; i < MAX_SESH; i++){
            if(sesh_list[i] != NULL){
                if(first != 0){
                    sprintf(response.data + strlen(response.data), ", ");
                    response.size += sizeof(", ");
                }
                first = 1;
                sprintf(response.data + strlen(response.data), sesh_list[i]->session_id);
                response.size += sizeof(sesh_list[i]->session_id);
            }
        }
        if(send(req_sock, &response, sizeof(response), 0) < 0){
            printf("Can't send\n");
            return -1;
        }
    //otherwise broadcast text
    }else{
        int key = sock_key_search(req_sock);
        if(client_list[key]->in_session == 1){
            int sesh_key = find_sesh_key(client_list[key]->session_id); // client_list[key]->session_id = Luis and not actual session id
            struct session_part * p = sesh_list[sesh_key]->head; // this is seg faulting, because sesh_key = -1
            while(p != NULL){
                if(p->client_sock != req_sock){
                    printf("(DEBUG) MESSAGE:\nType: %d, Size: %d, Source: %s, Data: %s, Source Len: %s\n", msg->type, msg->size, msg->source, msg->data, strlen(msg->source));
                    struct message send_msg;
                    send_msg.type = MESSAGE;
                    strcpy(send_msg.source, msg->source);
                    send_msg.size = msg->size;
                    strcpy(send_msg.data, msg->data);
                    if(send(p->client_sock, &send_msg, sizeof(send_msg), 0) < 0){
                        printf("Can't send\n");
                        return -1;
                    }
                }
                p = p->next_part;
            }
        } 
    }
    return 0;
}



void read_database(){
    FILE    *textfile;
    char    line[MAXLINELENGTH];
    int ok;
    textfile = fopen("serv/database.txt", "r");
     
    while(fgets(line, MAXLINELENGTH, textfile)){
        int space = 0;
        char client[MAX_NAME];
        char password[MAX_PASS];
        for(int i = 0; i < MAXLINELENGTH; i++){
            if(isspace(line[i]) != 0){
               space = 1; 
               i++;
            }
            if(space = 0){
                client[i] = line[i];
            }else{
                password[i] = line[i];
            }
        }
        int ret = empty_key_search(0);
        if(ret == -1){
            return;
        }else{
            client_list[ret] = (struct client*)malloc(sizeof(struct client));
            client_list[ret]->client_key = ret;
            client_list[ret]->ID = strdup(client);
            client_list[ret]->password = strdup(password);
            client_list[ret]->session_id = NULL;
            client_list[ret]->logged_in = 0;
            client_list[ret]->in_session = 0;
        }
    }
     
    fclose(textfile);
}

void write_database(int key){
    FILE    *textfile;
    char    line[MAXLINELENGTH];

    textfile = fopen("serv/database.txt", "a");
    for(int i = 0; i < MAX_CLIENT; i++){
        if(client_list[i] != NULL){
            if(client_list[i]->client_key == key){
                sprintf(line, client_list[i]->ID);
                sprintf(line + strlen(line), " ");
                sprintf(line + strlen(line), client_list[i]->password);
                sprintf(line + strlen(line), "\n");
                fprintf(textfile, "%s", line);
            }
            
        }
    }
}
//sets up the database of clients and sessions
 void setup(){
    for(int i = 0; i < MAX_CLIENT; i++){
        client_list[i] = NULL;
    }

    for(int i = 0; i < MAX_SESH; i++){
        sesh_list[i] = NULL;
    }
    //sets up some random clients (hardcoded)
    
    read_database();
    return;
}

//returns 0 if login/signup successful, returns errorcode if login/signup unsuccessful 
int log_sign(int fd, int type, char* source, char* message){
    //Login
    if(type == LOGIN){
        int key;
        //if the user isn't in the database return unsuccessfully
        if((key = source_key_search(source)) == -1){
            return NOT_IN_DATABASE;
        }else{
            //if the password is wrong return unsuccessfully
            if(strcmp(client_list[key]->password, message) != 0){
                return WRONG_PASS;
            //if client is already logged in return unsuccessfully
            }else if(client_list[key]->logged_in == 1){
                return ALREADY_LOGGED;
            //otherwise, mark the client as logged in
            }else{
                client_list[key]->logged_in = 1;
                client_list[key]->client_sock = fd;
                client_list[key]->last_request = clock();
                return 0;
            }
        }
    }else if(type == SIGNUP){
        int ret = empty_key_search(0);
        if(ret == -1){
            return CLIENTS_MAXED;
        }else{    
            client_list[ret] = (struct client*)malloc(sizeof(struct client));
            client_list[ret]->client_key = ret;
            client_list[ret]->ID = strdup(source);
            client_list[ret]->password = strdup(message);
            client_list[ret]->session_id = NULL;
            client_list[ret]->logged_in = 1;
            client_list[ret]->in_session = 0;
            client_list[ret]->client_sock = fd;
            client_list[ret]->last_request = clock();
            write_database(ret);
            return 0;
        }
    }
    return UNKNOWN_ERROR;
}

int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr, "usage: conference_server <port number>");
    }
    /* Set Up Listener on specified port */
    setup();
    char* listening_on = argv[1];
    int i,j, listen_sockfd, newfd, addrlen;
    struct addrinfo hints, *servinfo, *p, *their_socket;
    int rv;
    int numbytes;
    fd_set read_fds, master;
    int fdmax;

    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    char remoteIP[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; //set hints to use AF_INET to use IPv4
    hints.ai_socktype = SOCK_STREAM; //use stream TCP socket
    hints.ai_flags = AI_PASSIVE; //use my IP
        
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    //error checking for getaddrinfo
    //getaddreinfo returns list of socket addressinfo structures that can be 
    //connected to based on criteria in hints
    if((rv = getaddrinfo(NULL, listening_on, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
    
    //loop through all possible addresses and bind to the first one we can
    for(p = servinfo; p != NULL; p=p->ai_next){
        //create file descriptor for socket based on addressinfo structure we chose
        if((listen_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("listener: socket");
            continue;
        }
        
        //bind the file descriptor to the socket address
        if(bind(listen_sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(listen_sockfd);
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
    
    printf("listener: listening...\n");

    if((listen(listen_sockfd, BACKLOGSIZE) == -1)){
        perror("listener: listen");
        exit(3);
    }


    // add the listener to the master set
    FD_SET(listen_sockfd, &master);
    FD_SET(STDIN_FILENO, &master);

    // keep track of the biggest file descriptor
    fdmax = listen_sockfd; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        clock_t current_time = clock();
        for(int i = 0; i < MAX_CLIENT; i++){
            if(client_list[i] != NULL && client_list[i]->logged_in == 1){
                double idle_time = (((double)current_time - client_list[i]->last_request)/CLOCKS_PER_SEC)/60;
                if(idle_time > TIMEOUT){
                    struct message response;
                    response.type = KICKED;
                    strcpy(response.data, "Kicked for being idle too long");
                    strcpy(response.source, "server");
                    response.size = sizeof("Kicked for being idle too long");
                    if(send(client_list[i]->client_sock, &response, sizeof(response), 0) < 0){
                        printf("Can't send\n");
                        return -1;
                    }
                    leave_sesh(i, client_list[i]->ID);
                    //set the client's logout bit
                    client_list[i]->logged_in = 0;
                    close(client_list[i]->client_sock);       
                    FD_CLR(client_list[i]->client_sock, &master);
                    client_list[i]->client_sock = -1;
                   
                }
            }
        }
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listen_sockfd) {
                    
                    // handle new connections
                    addrlen = sizeof their_addr;
                    
                    newfd = accept(listen_sockfd,
                        (struct sockaddr *)&their_addr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                        continue;
                    } else {
                        struct message msg;
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), remoteIP, INET6_ADDRSTRLEN), newfd);
                        if(recv(newfd, &msg, sizeof(msg), 0) < 0){
                            printf("Couldn't recieve\n");
                            return -1;
                        }

                        int ret = log_sign(newfd, msg.type, msg.source, msg.data);
                        
                        struct message response;

                        if(ret != 0){
                            response.type = LO_NAK;
                            if(ret == NOT_IN_DATABASE){
                                strcpy(response.data, "User not in database");
                                response.size = sizeof("User not in database") + 1;
                            }else if(ret == WRONG_PASS){
                                strcpy(response.data, "Wrong password");
                                response.size = sizeof("Wrong password") + 1;
                            }else if(ret == ALREADY_LOGGED){
                                strcpy(response.data, "User already logged in");
                                response.size = sizeof("User already logged in") + 1;
                            }else if(ret == CLIENTS_MAXED){
                                strcpy(response.data, "Sorry, there are too many users registered");
                                response.size = sizeof("Sorry, there are too many users registered") + 1;
                            }else{
                                strcpy(response.data, "Unknown Error");
                                response.size = sizeof("Unknown Error") + 1;
                            }
                        }else{
                            response.type = LO_ACK;
                            strcpy(response.data, "\0");
                            response.size = 1;
                        }
                        strcpy(response.source, "server");
                        
                        if(send(newfd, &response, sizeof(response), 0) < 0){
                            printf("Can't send\n");
                            return -1;
                        }

                        //if the login was unsuccessful, close the connection with the client. We're not listening to them now
                        if(ret != 0){
                            close(newfd);
                            FD_CLR(newfd, &master);
                        }
                    }
                //if there was input from the terminal, handle it:
                } else if(i == STDIN_FILENO) {
                    char buf[MAX_DATA];
                    fgets(buf, MAX_DATA, stdin);
                    if(strcmp(buf, "/exit\n") == 0){
                        printf("Shutting down the server\n");
                        //close all the socket file descriptors
                            for(int j = 0; j <= fdmax; j++) {
                                close(j);
                                FD_CLR(j, &master);
                            }
                            //log out all clients, clear their sessions, and drop their sessions
                            for(int j; j < MAX_CLIENT; j++){
                                if(client_list[j] != NULL){
                                    client_list[j]->session_id = NULL;
                                    client_list[j]->logged_in = 0;
                                    client_list[j]->in_session = 0;
                                    client_list[j]->client_sock = -1; 
                                }                             
                            }

                            for(int j; j < MAX_SESH; j++){
                                if(sesh_list[j] != NULL){
                                    free(sesh_list[j]->session_id);
                                    while(sesh_list[j]->head != NULL){
                                        struct session_part* current = sesh_list[j]->head;
                                        sesh_list[j]->head = sesh_list[j]->head->next_part;
                                        current->next_part = NULL;
                                        free(current);
                                    }
                                    free(sesh_list[j]);
                                    sesh_list[j] = NULL;
                                }
                            }
                            return 0;
                    }
                    //FD_CLR(i, &read_fds);
                } else {
                    struct message msg;
                    // handle data from a client
                    if ((numbytes = recv(i, &msg, sizeof(msg), 0)) <= 0) {
                        // got error or connection closed by client
                        if (numbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        int key = sock_key_search(i);
                        client_list[key]->last_request = clock();
                        if(msg.type == EXIT){
                            //leave the session that the client is in (if any)
                            leave_sesh(i, msg.source);
                            //set the client's logout bit
                            int key = sock_key_search(i);
                            client_list[key]->logged_in = 0;
                            client_list[key]->client_sock = -1;
                            close(i);
                            FD_CLR(i, &master);
                        }else{
                            parse_request(i, &msg);
                        }
                        
                    }
                } // END handle data from client

            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    


    return 0;
}