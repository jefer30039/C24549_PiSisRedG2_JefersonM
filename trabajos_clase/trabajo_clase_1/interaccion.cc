#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

using namespace std;

// Estructura del mensaje (según el protocolo)
struct Mensaje {
    int emisor;
    int receptor;
    string tipo;   // leer, escribir, eliminar, respuesta
    string datos;
};

// Cola de mensajes compartida
queue<Mensaje> cola;
mutex mtx;
condition_variable cv;

// Función del servidor
void servidor() {
    while (true) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return !cola.empty(); });

        Mensaje msg = cola.front();
        cola.pop();
        lock.unlock();

        cout << "Servidor recibio mensaje de proceso "
             << msg.emisor << endl;

        if (msg.tipo == "escribir") {
            cout << "Servidor escribe: " << msg.datos << endl;
        }
        else if (msg.tipo == "leer") {
            cout << "Servidor lee archivo solicitado\n";
        }
        else if (msg.tipo == "eliminar") {
            cout << "Servidor elimina archivo: " << msg.datos << endl;
        }
        else if (msg.tipo == "salir") {
            cout << "Servidor finalizando...\n";
            break;
        }

        cout << endl;
    }
}

// Función del cliente
void cliente() {

    Mensaje m1{1, 0, "escribir", "Hola servidor"};
    Mensaje m2{1, 0, "leer", "archivo.txt"};
    Mensaje m3{1, 0, "eliminar", "archivo.txt"};
    Mensaje m4{1, 0, "salir", ""};

    Mensaje mensajes[] = {m1, m2, m3, m4};

    for (auto &m : mensajes) {
        {
            lock_guard<mutex> lock(mtx);
            cola.push(m);
        }

        cout << "Cliente envio mensaje tipo: "
             << m.tipo << endl;

        cv.notify_one();
        this_thread::sleep_for(chrono::seconds(1));
    }
}

int main() {

    thread tServidor(servidor);
    thread tCliente(cliente);

    tCliente.join();
    tServidor.join();

    return 0;
}