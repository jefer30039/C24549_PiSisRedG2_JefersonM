/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2025-i
  *
  *  SSL Socket class implementation
  *
 **/

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdexcept>
#include <cstring>
#include <cstdio>

#include "SSLSocket.h"

/**
  *  Class constructor (Client)
 **/
SSLSocket::SSLSocket( bool IPv6 ) {
   this->VSocket::Init( 's', IPv6 );
   this->context = nullptr;
   this->SSLStruct = nullptr;
   this->Init(); // Initialize as client by default
}

/**
  *  Class constructor (Server)
 **/
SSLSocket::SSLSocket( const char * certFileName, const char * keyFileName, bool IPv6 ) {
   this->VSocket::Init( 's', IPv6 );
   this->context = nullptr;
   this->SSLStruct = nullptr;
   
   this->InitContext( true ); // Server context
   this->LoadCertificates( certFileName, keyFileName );
}

/**
  *  Class constructor (for accepted connections)
 **/
SSLSocket::SSLSocket( int id ) {
   this->VSocket::Init( id );
   this->context = nullptr;
   this->SSLStruct = nullptr;
}

/**
  * Class destructor
 **/
SSLSocket::~SSLSocket() {
   if ( nullptr != this->SSLStruct ) {
      SSL_free( reinterpret_cast<SSL *>( this->SSLStruct ) );
   }
   if ( nullptr != this->context ) {
      SSL_CTX_free( reinterpret_cast<SSL_CTX *>( this->context ) );
   }
   this->Close();
}

/**
  *  InitContext
 **/
void SSLSocket::InitContext( bool serverContext ) {
   SSL_library_init();
   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();

   const SSL_METHOD * method;
   if ( serverContext ) {
      method = TLS_server_method();
   } else {
      method = TLS_client_method();
   }

   SSL_CTX * ctx = SSL_CTX_new( method );
   if ( nullptr == ctx ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::InitContext, SSL_CTX_new failed" );
   }

   this->context = reinterpret_cast<void *>( ctx );
}

/**
  *  Init
 **/
void SSLSocket::Init() {
   if ( nullptr == this->context ) {
      this->InitContext( false ); // Client context by default
   }

   SSL * ssl = SSL_new( reinterpret_cast<SSL_CTX *>( this->context ) );
   if ( nullptr == ssl ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Init, SSL_new failed" );
   }

   this->SSLStruct = reinterpret_cast<void *>( ssl );
}

/**
  *  Load certificates
 **/
void SSLSocket::LoadCertificates( const char * certFileName, const char * keyFileName ) {
   SSL_CTX * ctx = reinterpret_cast<SSL_CTX *>( this->context );

   if ( SSL_CTX_use_certificate_file( ctx, certFileName, SSL_FILETYPE_PEM ) <= 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::LoadCertificates, SSL_CTX_use_certificate_file failed" );
   }

   if ( SSL_CTX_use_PrivateKey_file( ctx, keyFileName, SSL_FILETYPE_PEM ) <= 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::LoadCertificates, SSL_CTX_use_PrivateKey_file failed" );
   }

   if ( ! SSL_CTX_check_private_key( ctx ) ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::LoadCertificates, Private key does not match certificate" );
   }
}

/**
  *  Connect
 **/
int SSLSocket::Connect( const char * hostName, int port ) {
   int st = this->TryToConnect( hostName, port );
   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Connect, TCP connection failed" );
   }
   
   if ( nullptr == this->SSLStruct ) {
      this->Init();
   }

   SSL * ssl = reinterpret_cast<SSL *>( this->SSLStruct );
   SSL_set_fd( ssl, this->sockId );

   if ( SSL_connect( ssl ) <= 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Connect, SSL_connect failed" );
   }

   return st;
}

/**
  *  Connect (by service name)
 **/
int SSLSocket::Connect( const char * host, const char * service ) {
   int st = this->TryToConnect( host, service );
   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Connect, TCP connection failed" );
   }

   if ( nullptr == this->SSLStruct ) {
      this->Init();
   }

   SSL * ssl = reinterpret_cast<SSL *>( this->SSLStruct );
   SSL_set_fd( ssl, this->sockId );

   if ( SSL_connect( ssl ) <= 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Connect, SSL_connect failed" );
   }

   return st;
}

/**
  *  MakeConnection (for compatibility with SSLClient.cc)
 **/
void SSLSocket::MakeConnection( const char * host, int port ) {
   this->Connect( host, port );
}

/**
  *  Copy (for server to handle new client connections)
 **/
void SSLSocket::Copy( SSLSocket * original ) {
   // Use the server's context
   SSL * ssl = SSL_new( reinterpret_cast<SSL_CTX *>( original->context ) );
   if ( nullptr == ssl ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Copy, SSL_new failed" );
   }

   this->SSLStruct = reinterpret_cast<void *>( ssl );
   SSL_set_fd( ssl, this->sockId );
}

/**
  *  Accept (SSL handshake)
 **/
void SSLSocket::Accept() {
   if ( nullptr == this->SSLStruct ) {
      throw std::runtime_error( "SSLSocket::Accept, SSL structure not initialized" );
   }

   if ( SSL_accept( reinterpret_cast<SSL *>( this->SSLStruct ) ) <= 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Accept, SSL_accept failed" );
   }
}

/**
  *  Read
 **/
size_t SSLSocket::Read( void * buffer, size_t size ) {
   int st = SSL_read( reinterpret_cast<SSL *>( this->SSLStruct ), buffer, size );
   if ( st < 0 ) {
      ERR_print_errors_fp( stderr );
      throw std::runtime_error( "SSLSocket::Read failed" );
   }
   return static_cast<size_t>( st );
}

/**
  *  Write
 **/
size_t SSLSocket::Write( const void * buffer, size_t size ) {
   int st = SSL_write( reinterpret_cast<SSL *>( this->SSLStruct ), buffer, size );
   if ( st < 0 ) {
      throw std::runtime_error( "SSLSocket::Write failed" );
   }
   return static_cast<size_t>( st );
}

/**
  *  Write (string)
 **/
size_t SSLSocket::Write( const char * string ) {
   return this->Write( string, strlen( string ) );
}

/**
  *  ShowCerts
 **/
void SSLSocket::ShowCerts() {
   X509 * cert = SSL_get_peer_certificate( reinterpret_cast<SSL *>( this->SSLStruct ) );
   if ( nullptr != cert ) {
      printf( "Certificates:\n" );
      char * line = X509_NAME_oneline( X509_get_subject_name( cert ), 0, 0 );
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
  *  GetCipher
 **/
const char * SSLSocket::GetCipher() {
   return SSL_get_cipher( reinterpret_cast<SSL *>( this->SSLStruct ) );
}

/**
  *  AcceptConnection (override)
 **/
VSocket * SSLSocket::AcceptConnection() {
   int id = this->WaitForConnection();
   SSLSocket * peer = new SSLSocket( id );
   return peer;
}
