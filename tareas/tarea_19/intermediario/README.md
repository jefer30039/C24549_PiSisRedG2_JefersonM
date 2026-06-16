# Servidor Intermediario — Lego Project
### CI0123 Proyecto integrador de redes y sistemas operativos · ECCI · UCR · 2026-i

---

## Arquitectura

```
[legoclient.c / Navegador]
         │  HTTP (TCP 8081)
         ▼
 ┌────────────────────┐          ┌─────────────────────┐
 │  INTERMEDIARIO     │  Pt TCP  │  LegoServer          │
 │  (este servidor)   │◄────────►│  (servidor figuras)  │
 │                    │  8080    │                       │
 │  - Tabla de rutas  │          │  - FileSystem        │
 │  - UDP 3030 JOIN   │          │  - Protocolo Pt      │
 │  - TCP 3031 PEER   │          └─────────────────────┘
 └────────────────────┘
         │  TCP 3031 (HANDSHAKE / INTERMEDIARY_REQUEST)
         ▼
 [Otros intermediarios]
```

---

## Archivos

| Archivo                  | Descripcion                                              |
|--------------------------|----------------------------------------------------------|
| `IntermediaryServer.h`   | Declaracion de la clase y estructuras de datos           |
| `IntermediaryServer.cpp` | Implementacion completa del intermediario                |
| `mainintermediary.cpp`   | Punto de entrada (main), acepta argumentos CLI           |
| `Makefile`               | Compilacion del proyecto                                 |
| `VSocket.cc / .h`        | Clase base de sockets (copiada del servidor de figuras)  |
| `Socket.cc / .h`         | Clase concreta de sockets                                |
| `Logger.h`               | Logger con mutex para escritura segura en bitacora.log   |

---

## Compilacion

```bash
# Desde el directorio del intermediario:
make
```

Requiere tener copiados `VSocket.cc`, `VSocket.h`, `Socket.cc`, `Socket.h`, `Logger.h`
en el mismo directorio (se incluyen en esta entrega).

---

## Ejecucion

```bash
# Primero levantar el servidor de figuras (LegoServer) en su directorio:
./server   # escucha en 0.0.0.0:8080

# Luego levantar el intermediario:
./intermediario [bindIp] [clientPort] [figServerIp] [figServerPort]

# Ejemplo con valores por defecto:
./intermediario 0.0.0.0 8081 127.0.0.1 8080
```

Los clientes NachOS deben apuntar al puerto **8081** (el del intermediario), no al 8080.

---

## Tabla de rutas

Al iniciar, el intermediario envía `P/R/dir\n` al servidor de figuras, obtiene la lista
de figuras disponibles y construye la tabla de rutas con el formato:

```
========= TABLA DE RUTAS =========
Figura               IP Servidor    Puerto
----------------------------------
Fish                 127.0.0.1      8080
Giraffe              127.0.0.1      8080
House                127.0.0.1      8080
Orchid               127.0.0.1      8080
==================================
```

Cada entrada contiene:
- **Nombre de la figura** (key de búsqueda)
- **IP del servidor** que la tiene
- **Puerto** de ese servidor

---

## Protocolo con el servidor de figuras (Protocolo Pt)

El intermediario usa el subprotocolo intragrupal ya implementado en `LegoServer`:

| Mensaje enviado         | Significado                        | Respuesta esperada     |
|-------------------------|------------------------------------|------------------------|
| `P/R/dir\n`             | Pedir lista de figuras             | `P/D/fig1,fig2,...`    |
| `P/G/<figura>:1\n`      | Pedir parte 1 de una figura        | `P/D/qty|desc,qty|desc,...` |
| `P/G/<figura>:2\n`      | Pedir parte 2 de una figura        | `P/D/qty|desc,...`     |
| `P/G/<figura>\n`        | Pedir ambas partes                 | `P/D/qty|desc,...`     |

Si la figura no existe: `P/D/404`

---

## Protocolo inter-intermediario (ProtocoloPt.md)

### Puertos fijos
| Puerto | Protocolo | Propósito                                |
|--------|-----------|------------------------------------------|
| 3030   | UDP       | Recibir paquetes JOIN de otros forks     |
| 3031   | TCP       | Recibir HANDSHAKE e INTERMEDIARY_REQUEST |

### Flujo de descubrimiento
1. Otro intermediario envía UDP JOIN al puerto 3030.
2. Este intermediario abre TCP al puerto 3031 del remitente.
3. Intercambian paquetes HANDSHAKE con sus listas de figuras.
4. Ambos actualizan su tabla de peers.

### Formato de paquetes binarios

**JOIN (UDP, tipo=0)**
```
uint8_t  tipo = 0
in_addr  sourceIp
```

**HANDSHAKE (TCP, tipo=1)**
```
uint8_t  tipo = 1
uint32_t contentLength
char[n]  content  ← "lion,giraffe,shark"
```

**INTERMEDIARY_REQUEST (TCP, tipo=2)**
```
uint8_t  tipo = 2
uint8_t  half        (1=primera, 2=segunda, 3=todas)
uint8_t  nameLength
char[n]  figureName
```

**INTERMEDIARY_RESPONSE (TCP, tipo=3)**
```
uint8_t  tipo = 3
uint8_t  half
uint8_t  figureNameLength
char[n]  figureName
uint32_t contentLength
char[n]  content     ← "[qty,nombre][qty,nombre]..."
```

**FIGURE_NOT_FOUND (TCP, tipo=4)**
```
uint8_t  tipo = 4
uint32_t contentLength
char[n]  figureName
```

---

## Endpoints HTTP

| URL                                      | Descripcion                                  |
|------------------------------------------|----------------------------------------------|
| `GET /lego/index.php`                    | Lista de figuras + tabla de rutas (HTML)     |
| `GET /lego/list.php?figure=X&part=1`     | Piezas de figura X, parte 1 (HTML)           |
| `GET /lego/list.php?figure=X&part=2`     | Piezas de figura X, parte 2 (HTML)           |

Con `User-Agent: nachos`, las respuestas son texto plano en lugar de HTML:
- Index: una figura por línea
- List: `cantidad|descripcion\n` por línea

---

## Bitacora

Todos los eventos se registran en `bitacora.log` (mismo directorio donde se ejecuta)
y también en consola. Formato:

```
[INTERMEDIARY] [DD/MM/YYYY HH:MM] [TIPO] mensaje
```

Tipos de evento: `START`, `ROUTE_TABLE`, `ROUTE`, `HANDLE_INDEX`, `HANDLE_LIST`,
`FIGURE_FOUND`, `FIGURE_NOT_FOUND`, `PROTO_REQUEST`, `JOIN_RECEIVED`,
`HANDSHAKE_DONE`, `PEER_ADDED`, `IR_REQUEST`, `IR_RESPONSE`, `ERROR`.

---

## Prueba rapida

```bash
# Terminal 1: servidor de figuras
cd LegoServer/
./server

# Terminal 2: intermediario
cd intermediario/
./intermediario

# Terminal 3: cliente curl
curl http://127.0.0.1:8081/lego/index.php
curl "http://127.0.0.1:8081/lego/list.php?figure=House&part=1"

# Terminal 3 (NachOS simulado con curl):
curl -H "User-Agent: nachos" http://127.0.0.1:8081/lego/index.php
curl -H "User-Agent: nachos" "http://127.0.0.1:8081/lego/list.php?figure=House&part=2"
```
