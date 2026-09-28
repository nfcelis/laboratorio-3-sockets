#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000
#define BUFFER_SIZE 1024


/*
 * Envía completamente un mensaje TCP.
 *
 * send() no garantiza enviar todos los bytes en una única llamada.
 */
int send_all(
    SOCKET socket,
    const char *data,
    int length
) {

    int total_sent = 0;

    while (total_sent < length) {

        int sent = send(
            socket,
            data + total_sent,
            length - total_sent,
            0
        );

        if (sent == SOCKET_ERROR) {
            return -1;
        }

        total_sent += sent;
    }

    return 0;
}


int main() {

    WSADATA wsaData;

    SOCKET subscriber_socket;

    struct sockaddr_in broker_address;

    char topic[100];

    char subscription[150];


    printf("=== SUBSCRIBER TCP ===\n");


    /*
     * Inicializar Winsock.
     */
    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0) {

        printf(
            "Error inicializando Winsock.\n"
        );

        return 1;
    }


    /*
     * Crear socket TCP IPv4.
     */
    subscriber_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );


    if (subscriber_socket == INVALID_SOCKET) {

        printf(
            "Error creando socket: %d\n",
            WSAGetLastError()
        );

        WSACleanup();

        return 1;
    }


    /*
     * Configurar dirección del broker.
     */
    broker_address.sin_family =
        AF_INET;

    broker_address.sin_port =
        htons(PORT);


    /*
     * Convierte la dirección IPv4 escrita como texto
     * ("127.0.0.1") a representación binaria.
     */
    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &broker_address.sin_addr
        ) != 1) {

        printf(
            "Direccion IP invalida.\n"
        );

        closesocket(subscriber_socket);
        WSACleanup();

        return 1;
    }


    /*
     * connect() establece la conexión TCP con el broker.
     */
    if (connect(
            subscriber_socket,
            (struct sockaddr *)&broker_address,
            sizeof(broker_address)
        ) == SOCKET_ERROR) {

        printf(
            "Error conectando al broker: %d\n",
            WSAGetLastError()
        );

        closesocket(subscriber_socket);
        WSACleanup();

        return 1;
    }


    printf(
        "Conectado al broker %s:%d\n",
        SERVER_IP,
        PORT
    );


    /*
     * Solicitar el partido que desea seguir.
     */
    printf(
        "Partido al que desea suscribirse: "
    );

    fgets(
        topic,
        sizeof(topic),
        stdin
    );


    /*
     * Eliminar salto de línea producido por fgets().
     */
    topic[strcspn(topic, "\r\n")] = '\0';


    /*
     * Construir mensaje del protocolo:
     *
     * SUB|PARTIDO\n
     */
    snprintf(
        subscription,
        sizeof(subscription),
        "SUB|%s\n",
        topic
    );


    if (send_all(
            subscriber_socket,
            subscription,
            (int)strlen(subscription)
        ) == -1) {

        printf(
            "Error enviando suscripcion.\n"
        );

        closesocket(subscriber_socket);
        WSACleanup();

        return 1;
    }


    printf(
        "Suscrito a [%s]. Esperando noticias...\n",
        topic
    );


    /*
     * El subscriber permanece escuchando indefinidamente.
     */
    while (1) {

        char buffer[BUFFER_SIZE];


        int bytes_received = recv(
            subscriber_socket,
            buffer,
            sizeof(buffer) - 1,
            0
        );


        if (bytes_received == 0) {

            printf(
                "El broker cerro la conexion.\n"
            );

            break;
        }


        if (bytes_received == SOCKET_ERROR) {

            printf(
                "Error en recv: %d\n",
                WSAGetLastError()
            );

            break;
        }


        /*
         * recv() entrega bytes y no una cadena terminada en '\0'.
         * Añadimos manualmente el terminador.
         */
        buffer[bytes_received] = '\0';


        printf(
            "%s",
            buffer
        );
    }


    closesocket(subscriber_socket);

    WSACleanup();

    return 0;
}