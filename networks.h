
// 	Writen - HMS April 2017
//  Supports TCP and UDP - both client and server


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LISTEN_BACKLOG 10

typedef struct {
    int32_t socketNum;
    struct sockaddr_in6 remote;
    uint32_t len;
} Connection;

// for the TCP server side
int tcpServerSetup(int serverPort);
int tcpAccept(int mainServerSocket, int debugFlag);

// for the TCP client side
int tcpClientSetup(char * serverName, char * serverPort, int debugFlag);

// For UDP Server and Client
int udpServerSetup(Connection * connection, int serverPort);
int setupUdpClientToServer(Connection* connection, char * hostName, int serverPort);

#endif