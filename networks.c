
// Hugh Smith April 2017
// Network code to support TCP/UDP client and server connections

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"
#include "gethostbyname.h"



// This function sets the server socket. The function returns the server
// socket number and prints the port number to the screen.  

int tcpServerSetup(int serverPort)
{
	// Opens a server socket, binds that socket, prints out port, call listens
	// returns the mainServerSocket
	
	int mainServerSocket = 0;
	struct sockaddr_in6 serverAddress;     
	socklen_t serverAddressLen = sizeof(serverAddress);  

	mainServerSocket= socket(AF_INET6, SOCK_STREAM, 0);
	if(mainServerSocket < 0)
	{
		perror("socket call");
		exit(1);
	}

	memset(&serverAddress, 0, sizeof(struct sockaddr_in6));
	serverAddress.sin6_family= AF_INET6;         		
	serverAddress.sin6_addr = in6addr_any;   
	serverAddress.sin6_port= htons(serverPort);         

	// bind the name (address) to a port 
	if (bind(mainServerSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("bind call");
		exit(-1);
	}
	
	// get the port name and print it out
	if (getsockname(mainServerSocket, (struct sockaddr*)&serverAddress, &serverAddressLen) < 0)
	{
		perror("getsockname call");
		exit(-1);
	}

	if (listen(mainServerSocket, LISTEN_BACKLOG) < 0)
	{
		perror("listen call");
		exit(-1);
	}
	
	printf("Server Port Number %d \n", ntohs(serverAddress.sin6_port));
	
	return mainServerSocket;
}

// This function waits for a client to ask for services.  It returns
// the client socket number.   

int tcpAccept(int mainServerSocket, int debugFlag)
{
	struct sockaddr_in6 clientAddress;   
	int clientAddressSize = sizeof(clientAddress);
	int client_socket = 0;

	if ((client_socket = accept(mainServerSocket, (struct sockaddr*) &clientAddress, (socklen_t *) &clientAddressSize)) < 0)
	{
		perror("accept call");
		exit(-1);
	}
	  
	if (debugFlag)
	{
		printf("Client accepted.  Client IP: %s Client Port Number: %d\n",  
				getIPAddressString6(clientAddress.sin6_addr.s6_addr), ntohs(clientAddress.sin6_port));
	}
	

	return(client_socket);
}

// This funciton opens a TCP socket, and connects to the server
// returns the socket number to the server

int tcpClientSetup(char * serverName, char * serverPort, int debugFlag)
{
	// This is used by the client to connect to a server using TCP
	
	int socket_num;
	uint8_t * ipAddress = NULL;
	struct sockaddr_in6 serverAddress;      
	
	// create the socket
	if ((socket_num = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	{
		perror("socket call");
		exit(-1);
	}

	// setup the server structure
	memset(&serverAddress, 0, sizeof(struct sockaddr_in6));
	serverAddress.sin6_family = AF_INET6;
	serverAddress.sin6_port = htons(atoi(serverPort));
	
	// get the address of the server 
	if ((ipAddress = gethostbyname6(serverName, &serverAddress)) == NULL)
	{
		exit(-1);
	}

	if(connect(socket_num, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("connect call");
		exit(-1);
	}

	if (debugFlag)
	{
		printf("Connected to %s IP: %s Port Number: %d\n", serverName, getIPAddressString6(ipAddress), atoi(serverPort));
	}
	
	return socket_num;
}

// This funciton creates a UDP socket on the server side and binds to that socket.  
// It prints out the port number and returns the socket number 

int udpServerSetup(Connection* connection, int serverPort)
{
	connection->socketNum = 0;
	connection->len = 0;
	
	// create the socket
	if ((connection->socketNum = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("socket() call error");
		exit(-1);
	}

	// set up the socket
	memset(&(connection->remote), 0, sizeof(struct sockaddr_in6));
	connection->remote.sin6_family = AF_INET6; // internet (IPv6 or IPv4) family
	connection->remote.sin6_addr = in6addr_any; // use any local IP address
	connection->remote.sin6_port = htons(serverPort); // if 0, OS picks

	// bind the name (address) to a port
	if (bind(connection->socketNum, (struct sockaddr *) &(connection->remote), sizeof(struct sockaddr_in6)) < 0) {
		perror("bind() call error");
		exit(-1);
	}

	connection->len = sizeof(struct sockaddr_in6);
	if (getsockname(connection->socketNum, (struct sockaddr *) &(connection->remote), (socklen_t *) &(connection->len)) < 0) {
		printf("Cannot retrieve port number\n");
		return -1;
	}
	printf("Server using Port #: %d\n", ntohs(connection->remote.sin6_port));

	return 0;
	
}

// This function opens a socket and fills in the Connection structure using the hostName and serverPort.  
// It assumes the address structure is created before calling this.
// Returns the socket number and the filled in Connection struct.

int setupUdpClientToServer(Connection* connection, char * hostName, int serverPort)
{
	connection->socketNum = 0;
	connection->len = sizeof(struct sockaddr_in6);
	char ipString[INET6_ADDRSTRLEN];
	uint8_t * ipAddress = NULL;
	
	// create the socket
	if ((connection->socketNum = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket() call error");
		exit(-1);
	}
	
	memset(&(connection->remote), 0, sizeof(struct sockaddr_in6));
	connection->remote.sin6_port = htons(serverPort);
	connection->remote.sin6_family = AF_INET6;

	if ((ipAddress = gethostbyname6(hostName, &(connection->remote))) == NULL)
	{
		printf("Cannot connect to host %s\n", hostName);
		return -1;
	}
		
	
	inet_ntop(AF_INET6, ipAddress, ipString, sizeof(ipString));
	printf("Server info - IP: %s Port: %d \n", ipString, serverPort);
		
	return 0;
}


