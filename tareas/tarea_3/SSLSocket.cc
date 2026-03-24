/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  *  SSL Socket class implementation
  *
  * (Fedora version)
  *
 **/
 
// SSL includes
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <stdexcept>

#include "SSLSocket.h"
#include "Socket.h"

/**
  *  Class constructor
  *     use base class
  *
  *  @param     char t: socket type to define
  *     's' for stream
  *     'd' for datagram
  *  @param     bool ipv6: if we need a IPv6 socket
  *
 **/
SSLSocket::SSLSocket( bool IPv6 ) {

   this->Init( 's', IPv6 );

   this->Context = nullptr;
   this->BIO = nullptr;

   this->InitSSL();					// Initializes to client context

}


/**
  *  Class constructor
  *     use base class
  *
  *  @param     char t: socket type to define
  *     's' for stream
  *     'd' for datagram
  *  @param     bool IPv6: if we need a IPv6 socket
  *
 **/
SSLSocket::SSLSocket( char * certFileName, char * keyFileName, bool IPv6 ) {

   this->Init( 's', IPv6 );

   this->Context = nullptr;
   this->BIO = nullptr;

   this->InitSSL( true );					// Initializes to server context
   this->LoadCertificates( certFileName, keyFileName );

}


/**
  *  Class constructor
  *
  *  @param     int id: socket descriptor
  *
 **/
SSLSocket::SSLSocket( int id ) {

   this->Init( id );

}


/**
  * Class destructor
  *
 **/
SSLSocket::~SSLSocket() {

// SSL destroy
   if ( nullptr != this->Context ) {
      SSL_CTX_free( reinterpret_cast<SSL_CTX *>( this->Context ) );
   }
   if ( nullptr != this->BIO ) {
      SSL_free( reinterpret_cast<SSL *>( this->BIO ) );
   }

   this->Close();

}


/**
  *  InitSSL
  *     use SSL_new with a defined context
  *
  *  Create a SSL object
  *
 **/
void SSLSocket::InitSSL( bool serverContext ) {

   this->InitContext( serverContext );

   SSL * ssl = SSL_new( reinterpret_cast<SSL_CTX *>( this->Context ) );
   if ( nullptr == ssl ) {
      throw std::runtime_error( "SSLSocket::InitSSL, SSL_new failed" );
   }

   this->BIO = reinterpret_cast<void *>( ssl );

}


/**
  *  InitContext
  *     use SSL_library_init, OpenSSL_add_all_algorithms, SSL_load_error_strings, TLS_server_method, SSL_CTX_new
  *
  *  Creates a new SSL server context to start encrypted comunications, this context is stored in class instance
  *
 **/
void SSLSocket::InitContext( bool serverContext ) {
   const SSL_METHOD * method;
   SSL_CTX * context;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
   SSL_library_init();
   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();
#endif

   if ( serverContext ) {
      method = TLS_server_method();
   } else {
      method = TLS_client_method();
   }

   context = SSL_CTX_new( method );
   if ( nullptr == context ) {
      throw std::runtime_error( "SSLSocket::InitContext, SSL_CTX_new failed" );
   }

   this->Context = reinterpret_cast<void *>( context );

}


/**
 *  Load certificates
 *    verify and load certificates
 *
 *  @param	const char * certFileName, file containing certificate
 *  @param	const char * keyFileName, file containing keys
 *
 **/
 void SSLSocket::LoadCertificates( const char * certFileName, const char * keyFileName ) {
    SSL_CTX * context = reinterpret_cast<SSL_CTX *>( this->Context );

    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file( context, certFileName, SSL_FILETYPE_PEM ) <= 0 ) {
        throw std::runtime_error( "SSLSocket::LoadCertificates, SSL_CTX_use_certificate_file failed" );
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file( context, keyFileName, SSL_FILETYPE_PEM ) <= 0 ) {
        throw std::runtime_error( "SSLSocket::LoadCertificates, SSL_CTX_use_PrivateKey_file failed" );
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key( context ) ) {
        throw std::runtime_error( "SSLSocket::LoadCertificates, Private key does not match the public certificate" );
    }
}
 

/**
 *  Connect
 *     use SSL_connect to establish a secure conection
 *
 *  Create a SSL connection
 *
 *  @param	char * hostName, host name
 *  @param	int port, service number
 *
 **/
int SSLSocket::Connect( const char * hostName, int port ) {
   int st;

   st = this->TryToConnect( hostName, port );		// Establish a non ssl connection first

   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   SSL_set_fd( ssl, this->sockId );

   if ( SSL_connect( ssl ) <= 0 ) {
      throw std::runtime_error( "SSLSocket::Connect, SSL_connect failed" );
   }

   return st;

}


/**
 *  Connect
 *     use SSL_connect to establish a secure conection
 *
 *  Create a SSL connection
 *
 *  @param	char * hostName, host name
 *  @param	char * service, service name
 *
 **/
int SSLSocket::Connect( const char * host, const char * service ) {
   int st;

   st = this->TryToConnect( host, service );

   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   SSL_set_fd( ssl, this->sockId );

   if ( SSL_connect( ssl ) <= 0 ) {
      throw std::runtime_error( "SSLSocket::Connect, SSL_connect failed" );
   }

   return st;

}


/**
  *  Read
  *     use SSL_read to read data from an encrypted channel
  *
  *  @param	void * buffer to store data read
  *  @param	size_t size, buffer's capacity
  *
  *  @return	size_t byte quantity read
  *
  *  Reads data from secure channel
  *
 **/
size_t SSLSocket::Read( void * buffer, size_t size ) {
   int st = -1;

   st = SSL_read( reinterpret_cast<SSL *>( this->BIO ), buffer, size );

   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Read( void *, size_t )" );
   }

   return (size_t) st;

}


/**
  *  Write
  *     use SSL_write to write data to an encrypted channel
  *
  *  @param	void * buffer to store data read
  *  @param	size_t size, buffer's capacity
  *
  *  @return	size_t byte quantity written
  *
  *  Writes data to a secure channel
  *
 **/
size_t SSLSocket::Write( const char * string ) {
   int st = -1;

   st = SSL_write( reinterpret_cast<SSL *>( this->BIO ), string, strlen( string ) );

   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Write( const char * )" );
   }

   return (size_t) st;

}


/**
  *  Write
  *     use SSL_write to write data to an encrypted channel
  *
  *  @param	const void * buffer to store data to write
  *  @param	size_t size, buffer's capacity
  *
  *  @return	size_t byte quantity written
  *
  *  Reads data from secure channel
  *
 **/
size_t SSLSocket::Write( const void * buffer, size_t size ) {
   int st = -1;

   st = SSL_write( reinterpret_cast<SSL *>( this->BIO ), buffer, size );

   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Write( void *, size_t )" );
   }

   return (size_t) st;

}


/**
 *   Show SSL certificates
 *
 **/
void SSLSocket::ShowCerts() {
   X509 *cert;
   char *line;

   cert = SSL_get_peer_certificate( reinterpret_cast<SSL *>( this->BIO ) );		 // Get certificates (if available)
   if ( nullptr != cert ) {
      printf("Server certificates:\n");
      line = X509_NAME_oneline( X509_get_subject_name( cert ), 0, 0 );
      printf( "Subject: %s\n", line );
      free( line );
      line = X509_NAME_oneline( X509_get_issuer_name( cert ), 0, 0 );
      printf( "Issuer: %s\n", line );
      free( line );
      X509_free( cert );
   } else {
      printf( "No certificates.\n" );
   }

}


/**
 *   Return the name of the currently used cipher
 *
 **/
const char * SSLSocket::GetCipher() {

   return SSL_get_cipher( reinterpret_cast<SSL *>( this->BIO ) );

}

