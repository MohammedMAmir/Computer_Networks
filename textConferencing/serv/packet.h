#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>  
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define MAX_PASS 20
#define MAX_PORT 20
#define MAX_COMMAND 400
#define MAX_NAME 100
#define MAX_DATA 4096
#define PART_MAX 20
#define MAX_SESSION_ID 100

// Message types
#define LOGIN 1
#define LO_ACK 2
#define LO_NAK 3
#define EXIT 4
#define JOIN 5
#define JN_ACK 6
#define JN_NAK 7
#define LEAVE_SESS 8
#define NEW_SESS 9
#define NS_ACK 10
#define MESSAGE 11
#define QUERY 12
#define QU_ACK 13
#define SIGNUP 14
#define NS_NAK 15
#define KICKED 16

//Error Codes:
#define NOT_IN_DATABASE -1
#define WRONG_PASS -2
#define ALREADY_LOGGED -3
#define CLIENTS_MAXED -4
#define UNKNOWN_ERROR -5
#define IN_SESSION -6
#define SESH_MAXED -7
#define SESH_FULL -8
#define NO_SESH -9

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

struct client{
    int client_key;     // Hash key to index client id into a hashtable
    char* ID;           // Id of client
    char* password;     // Dynamic password of client
    int logged_in;      // Bit to indicate if client is already logged in or not
    int in_session;     // Bit to indicate if client is in a session or not
    char* session_id;   // Character array that indicates which session client is in. Set to NULL if not in session
    int client_sock;    // The socket that this client is communicating on
    clock_t last_request; //stores the last time that the client made a request
};

struct session_part{
    int client_sock;
    int client_key;
    struct session_part* next_part;
};

struct session{
    int session_key;             //Hash key to index session id into a hashtable
    char* session_id;            //Character string session id
    int participants;            //Number of participants in session
    struct session_part* head;   //Keeps track of sockets in session
};
#endif
