#include "IntermediaryServer.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char * argv[])
{
    // Valores por defecto
    std::string bindIp        = "0.0.0.0";
    int         clientPort    = 8081;
    std::string figServerIp   = "127.0.0.1";
    int         figServerPort = 8080;

    if (argc >= 2) bindIp        = argv[1];
    if (argc >= 3) clientPort    = std::atoi(argv[2]);
    if (argc >= 4) figServerIp   = argv[3];
    if (argc >= 5) figServerPort = std::atoi(argv[4]);

    std::cout << "=== Servidor Intermediario Lego ===\n"
              << "  Clientes en:         " << bindIp << ":" << clientPort << "\n"
              << "  Servidor de figuras: " << figServerIp << ":" << figServerPort << "\n"
              << "  UDP JOIN en:         *:3030\n"
              << "  TCP Peers en:        " << bindIp << ":3031\n"
              << "===================================\n\n";

    try
    {
        IntermediaryServer server(bindIp, clientPort, figServerIp, figServerPort);
        server.run();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Error fatal del intermediario: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
