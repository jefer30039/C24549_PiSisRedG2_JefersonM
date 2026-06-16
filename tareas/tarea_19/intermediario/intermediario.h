#ifndef intermediario
#define intermediario

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include "Socket.h"

// Tipos de paquete del protocolo inter-intermediario
static constexpr uint8_t PKT_JOIN = 0;
static constexpr uint8_t PKT_HANDSHAKE = 1;
static constexpr uint8_t PKT_IR_REQUEST = 2;
static constexpr uint8_t PKT_IR_RESPONSE = 3;
static constexpr uint8_t PKT_NOT_FOUND = 4;

// Entrada en la tabla de rutas
struct FigureRoute {
    std::string figureName;
    std::string serverIp;
    int         serverPort;
};

// Entrada para otros intermediarios conocidos
struct PeerInfo {
    std::string ip;
    std::vector<std::string> figures; // figuras que ese peer conoce
};

class IntermediaryServer {
public:
    IntermediaryServer( const std::string & bindIp,
                        int clientPort,
                        const std::string & figureServerIp,
                        int figureServerPort );
    void run();

private:
    // -- Configuracion --
    std::string bindIp;
    int         clientPort; // puerto donde atiende clientes (ej. 8081)
    std::string figureServerIp; // IP del servidor de figuras
    int         figureServerPort; // Puerto del servidor de figuras

    // Tabla de rutas local: figura -> ruta
    std::map<std::string, FigureRoute> routeTable;
    std::mutex routeTableMutex;

    // -- Tabla de peers (otros intermediarios) --
    std::vector<PeerInfo> peers;
    std::mutex peersMutex;

    // -- Metodos de inicializacion --
    void buildRouteTable();
    std::vector<std::string> fetchFigureList( const std::string & ip, int port );

    // -- Comunicacion con servidor de figuras (protocolo Pt) --
    std::string queryFigureServer( const std::string & ip, int port,
                                   const std::string & figureName,
                                   uint8_t half );
    std::string sendProtoMessage( const std::string & ip, int port,
                                  const std::string & msg );

    // -- Atencion de clientes HTTP / NachOS --
    void listenClients();
    void handleClient( VSocket * client );
    std::string processHttpRequest( const std::string & request );
    std::string readRequest( VSocket * client );

    // -- Handlers HTTP --
    std::string handleIndex( bool nachos );
    std::string handleList( const std::string & path, bool nachos );
    std::string buildHttpResponse( const std::string & body,
                                   const std::string & status = "200 OK",
                                   const std::string & ctype  = "text/html; charset=UTF-8" );

    // -- Protocolo inter-intermediario --
    void listenJoinUdp(); // UDP 3030: recibe JOIN de otros forks
    void listenPeerTcp(); // TCP 3031: recibe HANDSHAKE / INTERMEDIARY_REQUEST
    void handleHandshake( VSocket * peer );
    std::string buildHandshakePayload() const;
    void parseHandshake( const std::string & payload, const std::string & peerIp );

    // -- Helpers HTTP --
    std::string getPathFromRequest( const std::string & request );
    std::string getQueryParam( const std::string & path, const std::string & key );
    bool isNachosRequest( const std::string & request ) const;

    // -- Helpers de conversion de piezas --
    std::string protoDataToNachos( const std::string & protoData );
    std::string protoDataToHtml( const std::string & protoData,
                                 const std::string & figure,
                                 const std::string & part );

    // -- Log de tabla de rutas --
    void printRouteTable() const;
};

#endif // IntermediaryServer_h
