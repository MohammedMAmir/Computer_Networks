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
#include <linux/limits.h>
#include <time.h>
#include <math.h>

#define MAXBUFLEN 1000

struct packet {
    unsigned int total_frag; // total number of fragments of the file
    unsigned int frag_no; // sequence number of the fragment
    unsigned int size; // size of filedata
    char* filename;
    unsigned char filedata[1000];
};

#endif