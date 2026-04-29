CC = gcc
CXX = g++
CFLAGS = -g -Wall
CXXFLAGS = -g -Wall
LIBS = -lssl -lcrypto -lpthread

OBJS = VSocket.o Socket.o SSLSocket.o

all: SSLServer SSLClient SSLServerC SSLClientC

# C++ versions
SSLServer: SSLServer.cc $(OBJS)
	$(CXX) $(CXXFLAGS) -o SSLServer SSLServer.cc $(OBJS) $(LIBS)

SSLClient: SSLClient.cc $(OBJS)
	$(CXX) $(CXXFLAGS) -o SSLClient SSLClient.cc $(OBJS) $(LIBS)

# C versions
SSLServerC: SSLServer.c
	$(CC) $(CFLAGS) -o SSLServerC SSLServer.c $(LIBS)

SSLClientC: SSLClient.c
	$(CC) $(CFLAGS) -o SSLClientC SSLClient.c $(LIBS)

# Aliases
server: SSLServer
client: SSLClient

VSocket.o: VSocket.cc VSocket.h
	$(CXX) $(CXXFLAGS) -c VSocket.cc

Socket.o: Socket.cc Socket.h VSocket.h
	$(CXX) $(CXXFLAGS) -c Socket.cc

SSLSocket.o: SSLSocket.cc SSLSocket.h VSocket.h
	$(CXX) $(CXXFLAGS) -c SSLSocket.cc

clean:
	rm -f *.o SSLServer SSLClient SSLServerC SSLClientC
