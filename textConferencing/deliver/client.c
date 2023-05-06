#include "packet.h"
#include <pthread.h>

struct client current_client = {.client_key = -1, .client_sock = -1, .ID = NULL, .in_session = 0, .logged_in = 0, .password = NULL, .session_id = NULL};
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int is_main_thread_working = 1;
int kill_thread = 0;

// used to ensure that all bytes are sent through multiple packets if necessary
int sendall(int socketfd, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(socketfd, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

// Create an account or login for user
int server_connect(int sockid, char *clientID, char *password, int type) {
    system("clear");
    // Send username and password to server
    struct message msg;

    msg.type = type;
    strcpy(msg.source, clientID);
    msg.size = strlen(password) + 1;
    strcpy(msg.data, password);

    if (send(sockid, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        return -1;
    }
    
    printf("(DEBUG) Sending to server\n");
    // Receive response from server
    memset(&msg, 0, sizeof(msg));
    if (recv(sockid, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Receive failed.\n");
        return -1;
    }
    printf("(DEBUG) Server response:\nType: %d, Size: %d, Source: %s, Data: %s\n\n", msg.type, msg.size, msg.source, msg.data);
    pthread_mutex_lock(&client_mutex);
    if (msg.type == LO_ACK) {
        current_client.ID = (char*)malloc(strlen(clientID) * sizeof(char));
        strncpy(current_client.ID, clientID, strlen(clientID));
        current_client.password = (char*)malloc(strlen(password) * sizeof(char));
        strncpy(current_client.password, password, strlen(password));
        current_client.client_sock = sockid;
        current_client.logged_in = 1;
        pthread_mutex_unlock(&client_mutex);
        if (type == LOGIN) {
            printf("(CLIENT) Login successful!\n\n");
        } else {
            printf("(CLIENT) Signup successful!\n\n");
        }
        return 1;
    } else if (msg.type == LO_NAK) {
        if (strcmp(msg.data, "User already logged in") == 0) {
            printf("(CLIENT) You were already logged in, restoring access...\n\n");
            current_client.ID = (char*)malloc(strlen(clientID) * sizeof(char));
            strncpy(current_client.ID, msg.source, strlen(clientID));
            current_client.client_sock = sockid;
            current_client.logged_in = 1;
            pthread_mutex_unlock(&client_mutex);
            return 1;
        } else {
            printf("(CLIENT) Failed: %s\n\n", msg.data);
        }
    }
    pthread_mutex_unlock(&client_mutex);
    return 0;
}

// Logout of the server
int logout() {
    struct message msg;

    msg.type = EXIT;
    strcpy(msg.source, current_client.ID);
    msg.size = 0;

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    printf("(DEBUG) Sending to server\n");
    
    // Clear current client
    pthread_mutex_lock(&client_mutex);
    current_client.ID = NULL;
    current_client.password = NULL;
    current_client.session_id = NULL;
    current_client.in_session = 0;
    current_client.logged_in = 0;
    current_client.client_sock = -1;

    pthread_mutex_unlock(&client_mutex);
    return 1;
}

// Terminate the program and send exit to server
int quit() {
    // Send exit to server
    pthread_mutex_lock(&client_mutex);
    struct message msg;

    msg.type = EXIT;
    strcpy(msg.source, current_client.ID);
    msg.size = 0;

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    printf("(DEBUG) Sending to server\n");
    // Receive response from server
    // memset(&msg, 0, sizeof(msg));
    // if (recv(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
    //     printf("(DEBUG) Receive failed.\n");
    //     pthread_mutex_unlock(&client_mutex);
    //     return -1;
    // }
    pthread_mutex_unlock(&client_mutex);
    return 1;
}

/*
Type = NEW_SESS ==> Create a new conference session and join it
Type = JOIN ==> // Join the conference session with the given session ID
*/
int session(char *sessionID, int type) {
    pthread_mutex_lock(&client_mutex);
    // Send request to server
    struct message msg;

    msg.type = type;
    strcpy(msg.source, current_client.ID);
    msg.size = strlen(sessionID) + 1;
    strcpy(msg.data, sessionID);

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    printf("(DEBUG) Sending to server\n");
    // Receive response from server
    memset(&msg, 0, sizeof(msg));
    if (recv(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Receive failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }

    printf("Server response:\nType: %d, Size: %d, Source: %s, Data: %s\n", msg.type, msg.size, msg.source, msg.data);

    if (msg.type == NS_ACK || msg.type == JN_ACK) {
        current_client.in_session = 1;
        current_client.session_id = sessionID;
        if (msg.type == NS_ACK) {
             printf("Session \"%s\" created.\n\n", sessionID);
        } else {
             printf("Successfully joined \"%s\" session.\n\n", sessionID);
        }
        pthread_mutex_unlock(&client_mutex);
        return 1;
    } else if (msg.type == JN_NAK) {
        printf("(CLIENT) Failed: %s\n\n", msg.data);
    } else {
        printf("(CLIENT) Failed to create new sessions\n");
    }
    pthread_mutex_unlock(&client_mutex);
    return 0;
}

// Leave current session
int leave_session() {
    pthread_mutex_lock(&client_mutex);
    if (current_client.in_session == 0) {
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    // Send request to server
    struct message msg;

    msg.type = LEAVE_SESS;
    strcpy(msg.source, current_client.ID);
    msg.size = 0;

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    printf("(DEBUG) Sending to server\n");
    current_client.in_session = 0;
    current_client.session_id = NULL;
    pthread_mutex_unlock(&client_mutex);
    return 1;
}

// Get the list of the connected clients and available sessions
int list() {
    pthread_mutex_lock(&client_mutex);
    // Send request to server
    struct message msg;

    msg.type = QUERY;
    strcpy(msg.source, current_client.ID);
    msg.size = 0;

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    printf("(DEBUG) Sending to server\n");
    // Receive response from server
    memset(&msg, 0, sizeof(msg));
    if (recv(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Receive failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    pthread_mutex_unlock(&client_mutex);

    printf("Server response:\nType: %d, Size: %d, Source: %s, Data: %s\n\n", msg.type, msg.size, msg.source, msg.data);

    if (msg.type == QU_ACK) {
        printf("%s\n", msg.data);
        return 1;
    } else {
        printf("(DEBUG) Failed to list users and sessions\n");
    }
    return 0;
}

int send_message(char *message) {
    pthread_mutex_lock(&client_mutex);
    // Send message to server
    struct message msg;

    msg.type = MESSAGE;
    strcpy(msg.source, current_client.ID);
    msg.size = strlen(message) + 1;
    strcpy(msg.data, message);

    if (send(current_client.client_sock, &msg, sizeof(msg), 0) < 0) {
        printf("(DEBUG) Send failed.\n");
        pthread_mutex_unlock(&client_mutex);
        return 0;
    }
    
    //printf("(DEBUG) Sending to server\n");
    pthread_mutex_unlock(&client_mutex);
    return 1;
}

void *handle_messages() {
    while (1) {
        if (is_main_thread_working) {
            // Stop waiting for messages from the server and wait for main thread to finish work
            pthread_mutex_lock(&mutex);
            while(is_main_thread_working) {
                pthread_cond_wait(&cond, &mutex);
            }
            pthread_mutex_unlock(&mutex);
        } else {
            // Continue waiting for messages from the server
            struct message msg;

            msg.type = MESSAGE;
            strcpy(msg.source, "");
            msg.size = 0;
            strcpy(msg.data, "");

            pthread_mutex_lock(&client_mutex);
            int sockfd = current_client.client_sock;
            pthread_mutex_unlock(&client_mutex);

            if (is_main_thread_working) {
                pthread_mutex_lock(&mutex);
                while(is_main_thread_working) {
                    pthread_cond_wait(&cond, &mutex);
                }
                pthread_mutex_unlock(&mutex);
            }

            memset(&msg, 0, sizeof(msg));
            if (recv(sockfd, &msg, sizeof(msg), 0) < 0) {
                printf("(DEBUG) Receive failed.\n");
                return 0;
            }
            if (msg.type == KICKED) {
                printf("(CLIENT) You have been kicked from the server. Shutting down...");
                exit(0);
            }
            //printf("(DEBUG) CHILD THREAD MESSAGE:\nType: %d, Size: %d, Source: %s, Data: %s\n", msg.type, msg.size, msg.source, msg.data);
            printf("(%s) %s", msg.source, msg.data);

            if (kill_thread) {
                break;
            }
        }
    }
    return NULL;
}

void child_thread_state(int main_thread_status) {
    pthread_mutex_lock(&mutex);
    is_main_thread_working = main_thread_status;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char** argv) {
    struct addrinfo hints, *servinfo, *p;

    char input[MAX_DATA];
    char command[MAX_COMMAND];
    char username[MAX_NAME];
    char password[MAX_PASS];
    char server_ip[INET6_ADDRSTRLEN];
    char server_port[MAX_PORT];
    char sessionID[MAX_SESSION_ID];
    printf("Welcome! \n");
    printf("\n(CLIENT) Available commands:\n"
        "   - /login <username> <password> <server-IP> <server-port>\n"
        "   - /signup <username> <password> <server-IP> <server-port>\n"
        "   - /createsession <session-ID>\n"
        "   - /joinsession <session-ID>\n"
        "   - /leavesession\n"
        "   - /list\n"
        "   - /logout\n"
        "   - /quit\n");
    printf("If you want to see this list again, just use /help\n\n");

    // Create a thread to handle receiving chat messages from the server
    pthread_t thread;
    int result = pthread_create(&thread, NULL, handle_messages, NULL);
    if (result != 0) {
        printf("(DEBUG) Failed to create thread: %d\n", result);
        return 1;
    }

    while(1) {   
        fgets(input, sizeof(input), stdin);
        sscanf(input, "%s", command);
        pthread_mutex_lock(&client_mutex);
        //printf("(DEBUG) current_client.logged_in: %d\n", current_client.logged_in);
        //printf("(DEBUG) current_client.in_session: %d\n", current_client.in_session);
        int logged_in = current_client.logged_in;
        pthread_mutex_unlock(&client_mutex);

        if (strncmp(command, "/help", 5) == 0) {
            printf("\n(CLIENT) Available commands:\n"
                "   - /login <username> <password> <server-IP> <server-port>\n"
                "   - /signup <username> <password> <server-IP> <server-port>\n"
                "   - /createsession <session-ID>\n"
                "   - /joinsession <session-ID>\n"
                "   - /leavesession\n"
                "   - /list\n"
                "   - /logout\n"
                "   - /quit\n\n");
        } else if (!logged_in) {
            if ((strncmp(command, "/login", 6) == 0) || (strncmp(command, "/signup", 7) == 0)) {
                sscanf(input, "%s %s %s %s %s", command, username, password, server_ip, server_port);
                printf("%s:%s:%s:%s:%s\n", command, username, password, server_ip, server_port);

                // get ip and port from user
                memset(&hints, 0, sizeof(hints)); // empty the struct
                hints.ai_flags = AI_CANONNAME;
                hints.ai_family = AF_UNSPEC; // only IPv4
                hints.ai_socktype = SOCK_STREAM;

                if(strlen(server_ip) <= 5 && strncmp(server_ip, "ug", 2)) {
                    strcat(server_ip, ".eecg.utoronto.ca");
                }

                int status = getaddrinfo(server_ip, server_port, &hints, &servinfo);

                if (status != 0) {
                    printf("Hostname: %s, Port: %s\n", server_ip, server_port);
                    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
                    return status;
                }

                //printf("%s\n", servinfo->ai_canonname);
                int sockfd, numbytes;

                // loop through all the results and connect to the first we can  
                for(p = servinfo; p != NULL; p = p->ai_next){
                    if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
                        perror("client: socket");
                        continue;
                    }

                    if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
                        close(sockfd);
                        perror("client: connect");
                        continue;
                    }
                    
                    break;
                }

                if(p == NULL){
                    fprintf(stderr, "client: failed to connect\n");
                    return 2;
                }

                printf("client: connecting to %s\n", inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), server_ip, sizeof server_ip));
                
                if(strncmp(command, "/login", 6) == 0) {    
                    // Call login function with parsed arguments
                    server_connect(sockfd, username, password, LOGIN);
                }
                else if(strncmp(command, "/signup", 7) == 0) {                
                    // Call signup function with parsed arguments
                    server_connect(sockfd, username, password, SIGNUP);
                }
            } else if (strncmp(command, "/quit", 5) == 0) {
                printf("Program Terminated\n");
                return 0;
            } else {
                system("clear");
                printf("(CLIENT) Invalid action: you need to login or signup first\n");
            }
        } else if (logged_in) {
            pthread_mutex_lock(&client_mutex);
            int in_session = current_client.in_session;
            pthread_mutex_unlock(&client_mutex);

            if ((strncmp(command, "/login", 6) == 0) || (strncmp(command, "/signup", 7) == 0)) {
                printf("(CLIENT) You're already logged in\n");
            } else if (strncmp(command, "/logout", 7) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                if(logout()) {
                    system("clear");
                    printf("(CLIENT) Logging out... Goodbye.\n");
                } else {
                    printf("(DEBUG) LOGOUT ERROR\n");
                }
                freeaddrinfo(servinfo);
            } else if (strncmp(command, "/quit", 5) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                if (quit()) {
                    printf("Program Terminated\n");
                } else {
                    printf("(DEBUG) QUIT ERROR\n");
                }
                break;
            } else if (strncmp(command, "/createsession", 14) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                sscanf(input, "/createsession %s", sessionID);
                if (!session(sessionID, NEW_SESS)) {
                    printf("(DEBUG) CREATESESSION ERROR\n");
                }
            } else if (strncmp(command, "/joinsession", 12) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                sscanf(input, "/joinsession %s", sessionID);
                session(sessionID, JOIN);
            } else if (strncmp(command, "/list", 5) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                if (!list()) {
                    printf("(DEBUG) LIST ERROR\n");
                }
                printf("\n");
            } else if (strncmp(command, "/leavesession", 13) == 0) {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                if (leave_session()) {
                    printf("(CLIENT) Left Session\n\n");
                } else {
                    printf("(CLIENT) You're not in any session. Try one of the following commands:\n"
                            "   - /createsession <session-ID>\n"
                            "   - /joinsession <session-ID>\n\n");
                }
            } else if (strncmp(input, "/", 1) != 0) {
                // The user wants to send a message
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
                if (in_session) {
                    // Send message to server
                    if (!send_message(input)) {
                        printf("(DEBUG) SEND MESSAGE ERROR\n");
                    }
                } else {
                    printf("(CLIENT) You're not in any session. Try one of the following commands:\n"
                            "   - /createsession <session-ID>\n"
                            "   - /joinsession <session-ID>\n\n");
                }
            }
            else {
                system("clear");
                printf("(CLIENT) Invalid command.........\n\n");
            }

            pthread_mutex_lock(&client_mutex);
            in_session = current_client.in_session;
            pthread_mutex_unlock(&client_mutex);

            if (in_session) {
                // Allow the client to start receiving messages
                if (is_main_thread_working) {
                    child_thread_state(0);
                }
            } else {
                if (!is_main_thread_working) {
                    child_thread_state(1);
                }
            }
        }
    }
    pthread_mutex_lock(&client_mutex);
    kill_thread = 1;
    pthread_mutex_unlock(&client_mutex);
    pthread_join(thread, NULL);
    freeaddrinfo(servinfo);
    return 0;
}