/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2025-i
  *
  *  SSL Socket class interface
  *
 **/

#ifndef SSLSocket_h
#define SSLSocket_h

#include "VSocket.h"

class SSLSocket : public VSocket {

   public:
      SSLSocket( bool IPv6 = false );				// Client constructor
      SSLSocket( const char * certFileName, const char * keyFileName, bool IPv6 = false );		// Server constructor
      SSLSocket( int );						// Constructor for accepted connections
      ~SSLSocket();

      int Connect( const char *, int );
      int Connect( const char *, const char * );
      void MakeConnection( const char *, int );			// Alias or specific implementation for Connect

      void Init();						// Initialize SSL structure
      void Accept();						// SSL handshake (SSL_accept)
      void Copy( SSLSocket * );					// Copy context from server socket
      
      size_t Write( const char * );
      size_t Write( const void *, size_t );
      size_t Read( void *, size_t );
      
      void ShowCerts();
      const char * GetCipher();

      virtual VSocket * AcceptConnection();			// Override to return SSLSocket

   private:
      void InitSSL( bool = false );				// Defaults to client context
      void InitContext( bool );
      void LoadCertificates( const char *, const char * );

      void * context;						// SSL context (SSL_CTX *)
      void * SSLStruct;						// SSL structure (SSL *)

};

#endif
