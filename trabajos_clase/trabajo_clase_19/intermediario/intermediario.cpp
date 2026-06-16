#include "intermediario.h"
#include "Logger.h"

#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

// Constructor
IntermediaryServer::IntermediaryServer(const std::string & bindIp,
                                       int clientPort,
                                       const std::string & figureServerIp,
                                       int figureServerPort )
    : bindIp(bindIp),
      clientPort(clientPort),
      figureServerIp(figureServerIp),
      figureServerPort(figureServerPort)
    {
}

// run(): punto de entrada principal
void IntermediaryServer::run() {
    Logger::log("INTERMEDIARY", "START",
                "bindIp=" + bindIp +
                " clientPort=" + std::to_string(clientPort) +
                " figureServer=" + figureServerIp + ":" + std::to_string(figureServerPort));

    // 1. Construir tabla de rutas desde el servidor de figuras
    buildRouteTable();
    printRouteTable();

    // 2. Hilo: escuchar JOIN UDP en puerto 3030
    std::thread udpThread([this]() { listenJoinUdp(); });
    udpThread.detach();

    // 3. Hilo: escuchar HANDSHAKE/INTERMEDIARY_REQUEST TCP en puerto 3031
    std::thread peerThread([this]() { listenPeerTcp(); });
    peerThread.detach();

    // 4. Loop principal: atender clientes HTTP / NachOS
    listenClients();
}

// buildRouteTable(): Consulta P/R/dir al servidor de figuras para obtener la lista
void IntermediaryServer::buildRouteTable()
{
    Logger::log("INTERMEDIARY", "ROUTE_TABLE", "Construyendo tabla de rutas...");

    std::vector<std::string> figures = fetchFigureList(figureServerIp, figureServerPort);

    std::lock_guard<std::mutex> lock(routeTableMutex);
    for (const auto & fig : figures){
        FigureRoute route;
        route.figureName  = fig;
        route.serverIp    = figureServerIp;
        route.serverPort  = figureServerPort;
        routeTable[fig]   = route;
    }

    Logger::log("INTERMEDIARY", "ROUTE_TABLE",
                "Figuras registradas: " + std::to_string(routeTable.size()));
}

// fetchFigureList(): Envia P/R/dir al servidor y devuelve la lista de nombres.
std::vector<std::string> IntermediaryServer::fetchFigureList( const std::string & ip, int port ){
    std::string response = sendProtoMessage(ip, port, "P/R/dir\n");

    // Formato de respuesta esperado: P/D/fig1,fig2,fig3
    std::vector<std::string> figures;

    // Limpiar el mensaje
    // Buscar el tercer campo despues de P/D/
    if (response.size() < 5 || response.substr(0, 4) != "P/D/"){
        Logger::log("INTERMEDIARY", "ERROR", "Respuesta inesperada de P/R/dir: " + response);
        return figures;
    }

    std::string data = response.substr(4);

    // Eliminar \n y \r al final
    while (!data.empty() && (data.back() == '\n' || data.back() == '\r')){
        data.pop_back();
    }

    // Si data es "404" o esta vacia, no hay figuras
    if (data.empty() || data == "404"){
        return figures;
    }

    // Separar por comas
    std::istringstream ss(data);
    std::string token;
    while (std::getline(ss, token, ',')){
        if (!token.empty()){
            figures.push_back(token);
        }
    }

    return figures;
}

// sendProtoMessage(): Abre conexion TCP, envia un mensaje de protocolo Pt y devuelve
// la respuesta completa.
std::string IntermediaryServer::sendProtoMessage( const std::string & ip,
                                                   int port,
                                                   const std::string & msg )
{
    try{
        Socket s('s', false);
        s.Connect(ip.c_str(), port);
        s.Write(msg.c_str());

        char buffer[4096];
        std::string response;
        size_t bytesRead;

        while ((bytesRead = s.Read(buffer, sizeof(buffer) - 1)) > 0){
            buffer[bytesRead] = '\0';
            response += buffer;
            // El servidor cierra tras responder; Read retornara 0
        }

        s.Close();
        return response;
    }
    catch (const std::exception & e){
        Logger::log("INTERMEDIARY", "ERROR",
                    std::string("sendProtoMessage error: ") + e.what());
        return "";
    }
}

// queryFigureServer(): Consulta P/G/<figura>:<parte> al servidor correspondiente
// y devuelve el campo de datos (lista de piezas) del protocolo Pt.
// half: 1 = primera mitad, 2 = segunda mitad, 0 = ambas
std::string IntermediaryServer::queryFigureServer( const std::string & ip,
                                                    int port,
                                                    const std::string & figureName,
                                                    uint8_t half )
{
    std::string protoMsg;

    if (half == 0){
        protoMsg = "P/G/" + figureName + "\n";
    }else{
        protoMsg = "P/G/" + figureName + ":" + std::to_string(half) + "\n";
    }

    Logger::log("INTERMEDIARY", "PROTO_REQUEST",
                "Enviando: " + protoMsg.substr(0, protoMsg.size()-1) +
                " -> " + ip + ":" + std::to_string(port));

    std::string response = sendProtoMessage(ip, port, protoMsg);

    // Formato: P/D/<datos>
    if (response.size() < 4 || response.substr(0, 4) != "P/D/"){
        Logger::log("INTERMEDIARY", "ERROR",
                    "Respuesta inesperada de P/G: " + response);
        return "";
    }

    std::string data = response.substr(4);
    while (!data.empty() && (data.back() == '\n' || data.back() == '\r'))
        data.pop_back();

    // 404 = figura no encontrada
    if (data == "404" || data == "400")
        return "";

    return data;
}

// Imprime la tabla de rutas en consola y log.
void IntermediaryServer::printRouteTable() const
{
    std::cout << "\n========= TABLA DE RUTAS =========\n";
    std::cout << std::left
              << std::setw(20) << "Figura"
              << std::setw(16) << "IP Servidor"
              << "Puerto\n";
    std::cout << "----------------------------------\n";

    for (const auto & entry : routeTable){
        const FigureRoute & r = entry.second;
        std::cout << std::left
                  << std::setw(20) << r.figureName
                  << std::setw(16) << r.serverIp
                  << r.serverPort << "\n";
        Logger::log("INTERMEDIARY", "ROUTE",
                    "figura=" + r.figureName +
                    " ip=" + r.serverIp +
                    " puerto=" + std::to_string(r.serverPort));
    }
    std::cout << "==================================\n\n";
}

// listenClients(): Loop principal: acepta conexiones de clientes HTTP/NachOS.
void IntermediaryServer::listenClients(){
    Socket serverSock('s', false);
    serverSock.Bind(bindIp.c_str(), clientPort);
    serverSock.MarkPassive(20);

    std::cout << "Intermediario escuchando clientes en "
              << bindIp << ":" << clientPort << std::endl;
    Logger::log("INTERMEDIARY", "LISTEN",
                "Clientes en " + bindIp + ":" + std::to_string(clientPort));

    while (true)
    {
        VSocket * client = serverSock.AcceptConnection();
        std::thread t([this, client]() { handleClient(client); });
        t.detach();
    }
}

// handleClient(): Atiende un cliente individual.
void IntermediaryServer::handleClient( VSocket * client ){
    try{
        std::string request = readRequest(client);
        if (request.empty()){
        
            client->Close();
            delete client;
            return;
        }

        Logger::log("INTERMEDIARY", "CLIENT_REQUEST",
                    request.substr(0, request.find('\n')));

        std::string response = processHttpRequest(request);
        client->Write(response.c_str());
        client->Shutdown(SHUT_WR);
    }
    catch (const std::exception & e){
        Logger::log("INTERMEDIARY", "ERROR",
                    std::string("handleClient: ") + e.what());
        try{
            std::string body =
                "<html><body><h1>500 Internal Server Error</h1><p>" +
                std::string(e.what()) + "</p></body></html>";
            client->Write(buildHttpResponse(body, "500 Internal Server Error").c_str());
        }
        catch (...) {}
    }

    try { client->Close(); } catch (...) {}
    delete client;
}

// readRequest(): Lee la solicitud completa del socket.
std::string IntermediaryServer::readRequest( VSocket * client ){
    std::string request;
    char buffer[2048];
    size_t bytesRead;

    while ((bytesRead = client->Read(buffer, sizeof(buffer) - 1)) > 0){
        buffer[bytesRead] = '\0';
        request += buffer;

        if (request.find("\r\n\r\n") != std::string::npos){
            break;
        }
    }
    return request;
}

// processHttpRequest(): Rutea la solicitud HTTP segun la ruta.
std::string IntermediaryServer::processHttpRequest( const std::string & request ){
    bool nachos = isNachosRequest(request);
    std::string path;

    try{
        path = getPathFromRequest(request);
    }   
        catch (...)
    {
        return buildHttpResponse("<html><body><h1>400 Bad Request</h1></body></html>",
                                 "400 Bad Request");
    }

    if (path == "/lego/index.php" || path == "/"){
        return buildHttpResponse(handleIndex(nachos));
    }

    if (path.find("/lego/list.php") == 0){
        return buildHttpResponse(handleList(path, nachos));
    }

    return buildHttpResponse(
        "<html><body><h1>404 Not Found</h1></body></html>",
        "404 Not Found"); 
}

// handleIndex(): Devuelve la lista de figuras disponibles.
// Si nachos=true devuelve texto plano (una por linea).
std::string IntermediaryServer::handleIndex( bool nachos ){
    std::lock_guard<std::mutex> lock(routeTableMutex);

    Logger::log("INTERMEDIARY", "HANDLE_INDEX",
                "Figuras disponibles: " + std::to_string(routeTable.size()));

    if (nachos){
        std::string result;
        for (const auto & entry : routeTable){
            result += entry.first + "\n";
        }
        return result;
    }

    // Respuesta HTML
    std::ostringstream html;
    html << "<html><head><meta charset=\"UTF-8\"><title>Intermediario Lego</title>"
         << "<style>"
         << "body{font-family:Arial,sans-serif;margin:30px;background:#f0f4f8;color:#222;}"
         << "h1{color:#1a237e;} h2{color:#555;font-weight:normal;}"
         << ".card{background:white;padding:25px;border-radius:12px;"
         << "box-shadow:0 2px 8px rgba(0,0,0,.12);margin-bottom:20px;}"
         << "table{border-collapse:collapse;width:100%;margin-top:15px;}"
         << "th{background:#1a237e;color:white;padding:10px;text-align:left;}"
         << "td{border:1px solid #ddd;padding:10px;}"
         << "tr:nth-child(even){background:#f5f5f5;}"
         << ".badge{background:#e8eaf6;color:#1a237e;padding:3px 9px;"
         << "border-radius:12px;font-size:13px;font-weight:bold;}"
         << "a{color:#1a237e;text-decoration:none;font-weight:bold;}"
         << "a:hover{text-decoration:underline;}"
         << "</style></head><body>"
         << "<div class=\"card\">"
         << "<h1>Servidor Intermediario - Lego</h1>"
         << "<h2>Tabla de rutas: " << routeTable.size() << " figura(s) registrada(s)</h2>"
         << "<table>"
         << "<tr><th>Figura</th><th>Servidor IP</th><th>Puerto</th>"
         << "<th>Parte 1</th><th>Parte 2</th></tr>";

    for (const auto & entry : routeTable){
        const FigureRoute & r = entry.second;
        html << "<tr>"
             << "<td><span class=\"badge\">" << r.figureName << "</span></td>"
             << "<td>" << r.serverIp << "</td>"
             << "<td>" << r.serverPort << "</td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.figureName << "&part=1\">Ver parte 1</a></td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.figureName << "&part=2\">Ver parte 2</a></td>"
             << "</tr>";
    }

    html << "</table></div></body></html>";
    return html.str();
}

// handleList(): Consulta las piezas de una figura al servidor correspondiente.
std::string IntermediaryServer::handleList( const std::string & path, bool nachos ){
    std::string figure = getQueryParam(path, "figure");
    std::string partStr = getQueryParam(path, "part");
    uint8_t half = (partStr == "2") ? 2 : 1;

    Logger::log("INTERMEDIARY", "HANDLE_LIST",
                "figure=" + figure + " part=" + partStr);

    // Buscar figura en tabla de rutas
    std::string targetIp;
    int targetPort = 0;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(routeTableMutex);
        auto it = routeTable.find(figure);
        if (it != routeTable.end()){
            targetIp   = it->second.serverIp;
            targetPort = it->second.serverPort;
            found      = true;
        }
    }

    // Si no esta en tabla local, intentar con peers
    if (!found){
        Logger::log("INTERMEDIARY", "ROUTE_MISS",
                    "figura=" + figure + " no en tabla local. Buscando en peers...");

        std::lock_guard<std::mutex> plock(peersMutex);
        for (const auto & peer : peers){
            for (const auto & pf : peer.figures){
                if (pf == figure){
                    targetIp   = peer.ip;
                    targetPort = 3031; // puerto de inter-intermediario
                    found      = true;
                    Logger::log("INTERMEDIARY", "PEER_ROUTE",
                                "figura=" + figure + " encontrada en peer=" + peer.ip);
                    break;
                }
            }
            if (found){
                break;
            }
        }
    }

    if (!found){
        Logger::log("INTERMEDIARY", "FIGURE_NOT_FOUND", "figure=" + figure);

        if (nachos)
            return "FIGURE_NOT_FOUND\n";

        return "<html><body><h1>Figura no encontrada</h1>"
               "<p>La figura <b>" + figure + "</b> no existe en ningún servidor registrado.</p>"
               "</body></html>";
    }

    // Consultar al servidor de figuras con protocolo Pt
    std::string protoData = queryFigureServer(targetIp, targetPort, figure, half);

    if (protoData.empty()){

        Logger::log("INTERMEDIARY", "FIGURE_NOT_FOUND",
                    "figure=" + figure + " part=" + partStr + " (sin datos)");

        if (nachos){
            return "FIGURE_NOT_FOUND\n";
        }
        return "<html><body><h1>Figura no encontrada</h1>"
               "<p>No se encontraron piezas para <b>" + figure + "</b> parte " + partStr + "</p>"
               "</body></html>";
    }

    Logger::log("INTERMEDIARY", "FIGURE_FOUND",
                "figure=" + figure + " part=" + partStr +
                " server=" + targetIp + ":" + std::to_string(targetPort));

    if (nachos){
        return protoDataToNachos(protoData);
    }
    return protoDataToHtml(protoData, figure, partStr);
}

// protoDataToNachos(): Convierte "qty|desc,qty|desc" -> "qty|desc\nqty|desc\n"
// (formato que espera legoclient.c)
std::string IntermediaryServer::protoDataToNachos( const std::string & protoData ){
    std::string result;
    std::istringstream ss(protoData);
    std::string token;

    while (std::getline(ss, token, ',')){
        if (!token.empty()){
            result += token + "\n";
        }   
    }

    return result;
}

// protoDataToHtml(): Convierte "qty|desc,qty|desc" -> tabla HTML
std::string IntermediaryServer::protoDataToHtml( const std::string & protoData,
                                                  const std::string & figure,
                                                  const std::string & part ){
    std::ostringstream html;
    html << "<html><head><meta charset=\"UTF-8\"><title>Piezas - " << figure << "</title>"
         << "<style>"
         << "body{font-family:Arial,sans-serif;margin:30px;background:#f0f4f8;color:#222;}"
         << "h1{color:#1a237e;} h2{color:#555;font-weight:normal;}"
         << ".card{background:white;padding:25px;border-radius:12px;"
         << "box-shadow:0 2px 8px rgba(0,0,0,.12);}"
         << "table{border-collapse:collapse;width:100%;margin-top:15px;}"
         << "th{background:#1a237e;color:white;padding:10px;text-align:left;}"
         << "td{border:1px solid #ddd;padding:10px;}"
         << "tr:nth-child(even){background:#f5f5f5;}"
         << ".total{background:#e8eaf6;font-weight:bold;}"
         << "a{color:#1a237e;}"
         << "</style></head><body>"
         << "<div class=\"card\">"
         << "<h1>Piezas: " << figure << "</h1>"
         << "<h2>Parte: " << part << "</h2>"
         << "<p><a href=\"/lego/index.php\">&larr; Volver al listado</a></p>"
         << "<table>"
         << "<tr><th>Cantidad</th><th>Descripcion</th></tr>";

    int total = 0;
    std::istringstream ss(protoData);
    std::string token;

    while (std::getline(ss, token, ',')){
        size_t pipe = token.find('|');
        if (pipe == std::string::npos) continue;

        try{
            int qty = std::stoi(token.substr(0, pipe));
            std::string desc = token.substr(pipe + 1);
            total += qty;

            html << "<tr><td>" << qty << "</td><td>" << desc << "</td></tr>";
        }
        catch (...) { continue; }
    }

    html << "<tr class=\"total\"><td colspan=\"2\">Total: " << total << " piezas</td></tr>";
    html << "</table></div></body></html>";

    return html.str();
}

// buildHttpResponse(): Construye una respuesta HTTP.
std::string IntermediaryServer::buildHttpResponse( const std::string & body,
                                                    const std::string & status,
                                                    const std::string & ctype ){
    std::ostringstream r;
    r << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << ctype << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return r.str();
}

// getPathFromRequest(): Obtiene la ruta de una solicitud HTTP.
std::string IntermediaryServer::getPathFromRequest( const std::string & request ){
    std::istringstream s(request);
    std::string method, path, version;
    s >> method >> path >> version;

    if (method != "GET"){
        throw std::runtime_error("Metodo HTTP no soportado");
    }

    return path;
}

// getQueryParam(): Obtiene un parámetro de la ruta.
std::string IntermediaryServer::getQueryParam( const std::string & path,
                                                const std::string & key ){
    size_t q = path.find('?');
    if (q == std::string::npos){
        return "";
    }

    std::string query = path.substr(q + 1);
    std::string pattern = key + "=";

    size_t start = query.find(pattern);
    if (start == std::string::npos){
        return "";
    }

    start += pattern.size();
    size_t end = query.find('&', start);

    if (end == std::string::npos){
        return query.substr(start);
    }

    return query.substr(start, end - start);
}

// isNachosRequest(): Detecta si es una solicitud de NachOS.
bool IntermediaryServer::isNachosRequest( const std::string & request ) const {
    return request.find("User-Agent: nachos") != std::string::npos;
}

// PROTOCOLO INTER-INTERMEDIARIO

// listenJoinUdp(): Escucha paquetes JOIN de otros intermediarios por UDP 3030.
// Cuando recibe uno, lanza un hilo para hacer HANDSHAKE TCP.
void IntermediaryServer::listenJoinUdp(){
    // Crear socket UDP raw para escuchar en 3030
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        Logger::log("INTERMEDIARY", "ERROR", "No se pudo crear socket UDP para JOIN");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(3030);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        Logger::log("INTERMEDIARY", "ERROR", "No se pudo bind UDP 3030");
        close(sockfd);
        return;
    }

    Logger::log("INTERMEDIARY", "LISTEN", "UDP JOIN en puerto 3030");
    std::cout << "Intermediario escuchando JOIN UDP en puerto 3030\n";

    // Formato del paquete JOIN segun protocolo:
    //   uint8_t tipo = 0 (JOIN)
    //   in_addr sourceIp
    while (true){
        uint8_t pkt[6];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);

        ssize_t n = recvfrom(sockfd, pkt, sizeof(pkt), 0,
                             (struct sockaddr*)&sender, &slen);
        if (n < 5)  {
            continue; // paquete malformado
        }

        if (pkt[0] != PKT_JOIN) {
            continue;

        // Extraer IP del campo sourceIp
        struct in_addr srcAddr;
        memcpy(&srcAddr, &pkt[1], sizeof(srcAddr));

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &srcAddr, ipStr, sizeof(ipStr));

        std::string peerIp(ipStr);

        Logger::log("INTERMEDIARY", "JOIN_RECEIVED",
                    "Nuevo intermediario: " + peerIp);

        // Lanzar hilo para hacer HANDSHAKE TCP al peer por puerto 3031
        std::thread t([this, peerIp](){
            try{
                Socket tcpSock('s', false);
                tcpSock.Connect(peerIp.c_str(), 3031);

                // Enviar HANDSHAKE
                std::string payload = buildHandshakePayload();

                // Formato: uint8_t tipo | uint32_t len | char[n]
                uint8_t tipo = PKT_HANDSHAKE;
                uint32_t len = htonl((uint32_t)payload.size());

                tcpSock.Write(&tipo, 1);
                tcpSock.Write(&len, 4);
                if (!payload.empty()){
                    tcpSock.Write(payload.c_str());
                }

                // Leer HANDSHAKE del peer
                char hdrBuf[5];
                size_t rd = tcpSock.Read(hdrBuf, 5);
                if (rd >= 5 && (uint8_t)hdrBuf[0] == PKT_HANDSHAKE){
                    uint32_t plen;
                    memcpy(&plen, &hdrBuf[1], 4);
                    plen = ntohl(plen);

                    std::string peerPayload(plen, '\0');
                    if (plen > 0){
                        tcpSock.Read(&peerPayload[0], plen);
                    }

                    parseHandshake(peerPayload, peerIp);
                }

                tcpSock.Close();
                Logger::log("INTERMEDIARY", "HANDSHAKE_DONE",
                            "Con peer=" + peerIp);
            }
            catch (const std::exception & e) {
                Logger::log("INTERMEDIARY", "ERROR",
                            std::string("HANDSHAKE fallo con ") + peerIp + ": " + e.what());
            }
        });
        t.detach();
    }
}

// listenPeerTcp(): Escucha en TCP 3031:
//   - Paquetes HANDSHAKE de peers que recibieron nuestro JOIN
//   - Paquetes INTERMEDIARY_REQUEST
void IntermediaryServer::listenPeerTcp() {
    try {
        Socket serverSock('s', false);
        serverSock.Bind(bindIp.c_str(), 3031);
        serverSock.MarkPassive(10);

        Logger::log("INTERMEDIARY", "LISTEN", "TCP PEER en puerto 3031");
        std::cout << "Intermediario escuchando peers TCP en puerto 3031\n";

        while (true){
            VSocket * peer = serverSock.AcceptConnection();
            std::thread t([this, peer]() { handleHandshake(peer); });
            t.detach();
        }
    }
    catch (const std::exception & e) {
        Logger::log("INTERMEDIARY", "ERROR",
                    std::string("listenPeerTcp: ") + e.what());
    }
}

// handleHandshake(): Lee el primer byte para determinar el tipo de paquete.
// Si es HANDSHAKE: intercambia figuras con el peer.
// Si es INTERMEDIARY_REQUEST: procesa la solicitud de figura.
void IntermediaryServer::handleHandshake( VSocket * peer ){
    try {
        uint8_t tipo;
        peer->Read(&tipo, 1);

        if (tipo == PKT_HANDSHAKE){
            // Leer longitud y contenido
            uint32_t netLen;
            peer->Read(&netLen, 4);
            uint32_t len = ntohl(netLen);

            std::string payload(len, '\0');
            if (len > 0)
                peer->Read(&payload[0], len);

            // Obtener IP del peer (aproximacion: usar el mismo socket)
            // En un sistema real se usaria getpeername()
            std::string peerIp = "peer"; // placeholder
            parseHandshake(payload, peerIp);

            // Responder con nuestro HANDSHAKE
            std::string myPayload = buildHandshakePayload();
            uint8_t myTipo = PKT_HANDSHAKE;
            uint32_t myLen = htonl((uint32_t)myPayload.size());

            peer->Write(&myTipo, 1);
            peer->Write(&myLen, 4);
            if (!myPayload.empty()){
                peer->Write(myPayload.c_str());
            }

            Logger::log("INTERMEDIARY", "HANDSHAKE_RECV",
                        "Handshake completado. Figuras del peer: " + payload);
        }
        else if (tipo == PKT_IR_REQUEST){
            // Formato: uint8_t half | uint8_t nameLen | char[n]
            uint8_t half, nameLen;
            peer->Read(&half, 1);
            peer->Read(&nameLen, 1);

            std::string figureName(nameLen, '\0');
            if (nameLen > 0){
                peer->Read(&figureName[0], nameLen);
            }

            Logger::log("INTERMEDIARY", "IR_REQUEST",
                        "figura=" + figureName + " half=" + std::to_string(half));

            // Buscar en tabla local
            std::string protoData;
            std::string targetIp;
            int targetPort = 0;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(routeTableMutex);
                auto it = routeTable.find(figureName);
                if (it != routeTable.end()){
                    targetIp   = it->second.serverIp;
                    targetPort = it->second.serverPort;
                    found      = true;
                }
            }

            if (found){
                protoData = queryFigureServer(targetIp, targetPort, figureName, half);
            }

            if (!protoData.empty()){
                // Enviar INTERMEDIARY_RESPONSE
                // tipo | half | figureNameLen | figureName | contentLen(u32) | content
                uint8_t rTipo = PKT_IR_RESPONSE;
                uint8_t rHalf = half;
                uint8_t rNameLen = (uint8_t)figureName.size();
                uint32_t rContentLen = htonl((uint32_t)protoData.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rHalf, 1);
                peer->Write(&rNameLen, 1);
                peer->Write(figureName.c_str());
                peer->Write(&rContentLen, 4);
                peer->Write(protoData.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE",
                            "figura=" + figureName + " enviada");
            }
            else{
                // FIGURE_NOT_FOUND
                uint8_t rTipo = PKT_NOT_FOUND;
                uint32_t rContentLen = htonl((uint32_t)figureName.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rContentLen, 4);
                peer->Write(figureName.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE",
                            "FIGURE_NOT_FOUND figura=" + figureName);
            }
        }
    }
    catch (const std::exception & e) {
        Logger::log("INTERMEDIARY", "ERROR",
                    std::string("handleHandshake: ") + e.what());
    }

    try {
        peer->Close();
    } catch (...) {}
    delete peer;
}

// buildHandshakePayload(): Construye la lista de figuras locales separadas por coma.
// Formato: "lion, giraffe,shark"
std::string IntermediaryServer::buildHandshakePayload() const
{
    std::string payload;
    bool first = true;

    for (const auto & entry : routeTable) {
        if (!first) {
            payload += ",";
        }
        payload += entry.first;
        first = false;
    }

    return payload;
}

// parseHandshake(): Parsea la lista de figuras recibida en un HANDSHAKE y
// registra el peer en la tabla de peers.
void IntermediaryServer::parseHandshake( const std::string & payload,
                                          const std::string & peerIp ) {
    PeerInfo info;
    info.ip = peerIp;

    std::istringstream ss(payload);
    std::string token;

    while (std::getline(ss, token, ',')) {
        if (!token.empty()){
            info.figures.push_back(token);
        }
    }

    {
        std::lock_guard<std::mutex> lock(peersMutex);

        // Actualizar si ya existe el peer
        for (auto & p : peers) {
            if (p.ip == peerIp) {
                p.figures = info.figures;
                Logger::log("INTERMEDIARY", "PEER_UPDATED",
                            "ip=" + peerIp + " figuras=" + std::to_string(info.figures.size()));
                return;
            }
        }
        peers.push_back(info);
    }

    Logger::log("INTERMEDIARY", "PEER_ADDED",
                "ip=" + peerIp + " figuras=" + std::to_string(info.figures.size()));
}
