/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  ****** VSocket base class implementation
  *
  * (Fedora version)
  *
 **/

#include <sys/socket.h>
#include <arpa/inet.h>		// ntohs, htons
#include <stdexcept>            // runtime_error
#include <cstring>		// memset
#include <netdb.h>		// getaddrinfo, freeaddrinfo
#include <unistd.h>		// close
/*
#include <cstddef>
#include <cstdio>

//#include <sys/types.h>
*/
#include "VSocket.h"


/**
  *  Class creator (constructor)
  *     use Unix socket system call
  *
  *  @param     char t: socket type to define
  *     's' for stream
  *     'd' for datagram
  *  @param     bool ipv6: if we need a IPv6 socket
  *
 **/
void VSocket::Init( char t, bool IPv6 ){

   int domain = AF_INET;
   int type   = SOCK_STREAM;

   this->IPv6 = IPv6;
   this->type = t;

   if ( IPv6 ) {
      domain = AF_INET6;
   }

   if ( 'd' == t ) {
      type = SOCK_DGRAM;
   }

   this->sockId = socket( domain, type, 0 );

   if ( -1 == this->sockId ) {
      throw std::runtime_error( "VSocket::Init, socket system call failed" );
   }

}


/**
  * Class destructor
  *
  **/
VSocket::~VSocket() {

   this->Close();

}


/**
  * Close method
  *    use Unix close system call (once opened a socket is managed like a file in Unix)
  *
 **/
void VSocket::Close(){
   int st = close( this->sockId );

   if ( -1 == st ) {
      // throw std::runtime_error( "VSocket::Close()" );
   }

}


/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dot notation, example "10.84.166.62"
  * @param      int port: process address, example 80
  *
 **/
int VSocket::TryToConnect( const char * hostip, int port ) {

   int st = -1;
   struct sockaddr_in host4;

   memset( &host4, 0, sizeof( host4 ) );
   host4.sin_family = AF_INET;
   st = inet_pton( AF_INET, hostip, &host4.sin_addr );
   if ( 1 != st ) {
       throw std::runtime_error( "VSocket::TryToConnect, inet_pton failed" );
   }
   host4.sin_port = htons( port );

   st = connect( this->sockId, (sockaddr *) &host4, sizeof( host4 ) );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::TryToConnect, connect failed" );
   }

   return st;

}


/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dns notation, example "os.ecci.ucr.ac.cr"
  * @param      char * service: process address, example "http"
  *
 **/
int VSocket::TryToConnect( const char *host, const char *service ) {
   int st = -1;
   struct addrinfo hints, *result, *rp;

   memset( &hints, 0, sizeof( hints ) );
   hints.ai_family = ( this->IPv6 ) ? AF_INET6 : AF_INET;
   hints.ai_socktype = ( 'd' == this->type ) ? SOCK_DGRAM : SOCK_STREAM;
   hints.ai_flags = 0;
   hints.ai_protocol = 0;

   st = getaddrinfo( host, service, &hints, &result );
   if ( 0 != st ) {
       throw std::runtime_error( "VSocket::TryToConnect, getaddrinfo failed" );
   }

   for ( rp = result; rp != nullptr; rp = rp->ai_next ) {
       st = connect( this->sockId, rp->ai_addr, rp->ai_addrlen );
       if ( 0 == st ) {
           break;
       }
   }

   freeaddrinfo( result );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::TryToConnect, connect failed" );
   }

   return st;

}


/**
  * Bind method
  *    use "bind" Unix system call (man 3 bind) (server mode)
  *
  * @param      int port: bind a unamed socket to a port defined in sockaddr structure
  *
  *  Links the calling process to a service at port
  *
 **/
int VSocket::Bind( int port ) {
   int st = -1;
   struct sockaddr_in host4;

   memset( &host4, 0, sizeof( host4 ) );
   host4.sin_family      = AF_INET;
   host4.sin_addr.s_addr = htonl( INADDR_ANY );
   host4.sin_port        = htons( port );
   memset( host4.sin_zero, '\0', sizeof( host4.sin_zero ) );

   st = bind( this->sockId, (sockaddr *) &host4, sizeof( host4 ) );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Bind" );
   }

   return st;

}


/**
  *  sendTo method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to send data
  *
  *  Send data to another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::sendTo( const void * buffer, size_t size, void * addr ) {
   struct sockaddr * other = (struct sockaddr *) addr;
   socklen_t addrLen = sizeof( struct sockaddr );

   ssize_t st = sendto( this->sockId, buffer, size, 0, other, addrLen );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::sendTo" );
   }

   return (size_t) st;

}


/**
  *  recvFrom method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to receive from data
  *
  *  @return	size_t bytes received
  *
  *  Receive data from another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::recvFrom( void * buffer, size_t size, void * addr ) {
   struct sockaddr * other = (struct sockaddr *) addr;
   socklen_t addrLen = sizeof( struct sockaddr );

   ssize_t st = recvfrom( this->sockId, buffer, size, 0, other, &addrLen );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::recvFrom" );
   }

   return (size_t) st;

}


