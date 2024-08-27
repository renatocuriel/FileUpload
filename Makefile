# Makefile for CPE464 tcp and udp test code
# updated by Hugh Smith - April 2023

# all target makes UDP test code
# tcpAll target makes the TCP test code


CC= gcc
CFLAGS= -g -Wall
LIBS = 

OBJS = pdu.o networks.o gethostbyname.o pollLib.o safeUtil.o bufferLib.o queueLib.o

#uncomment next two lines if your using sendtoErr() library
LIBS += libcpe464.a.b.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_


all: udpAll

udpAll: rcopy server

rcopy: rcopy.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f rcopy server *.o
