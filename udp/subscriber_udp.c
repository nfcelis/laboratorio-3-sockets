#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5001
#define BUFFER_SIZE 1024


int main() {

    WSADATA wsaData;

    SOCKET subscriber_socket;

    struct sockaddr_in broker_address;

    char topic[100];

    char subscription[150];


    printf(
        "=== SUBSCRIBER UDP ===\n"
    );


    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0
    ) {

        printf(
            "Error inicializando Winsock.\n"
        );

        return 1;
    }


    /*
     * Crear socket UDP.
     */
    subscriber_socket = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP
    );


    if (
        subscriber_socket ==
        INVALID_SOCKET
    ) {

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


    if (
        inet_pton(
            AF_INET,
            SERVER_IP,
            &broker_address.sin_addr
        ) != 1
    ) {

        printf(
            "Direccion IP invalida.\n"
        );

        closesocket(
            subscriber_socket
        );

        WSACleanup();

        return 1;
    }


    printf(
        "Broker UDP: %s:%d\n",
        SERVER_IP,
        PORT
    );


    printf(
        "Partido al que desea suscribirse: "
    );


    fgets(
        topic,
        sizeof(topic),
        stdin
    );


    topic[
        strcspn(
            topic,
            "\r\n"
        )
    ] = '\0';


    /*
     * Construir:
     *
     * SUB|PARTIDO
     */
    snprintf(
        subscription,
        sizeof(subscription),
        "SUB|%s",
        topic
    );


    /*
     * sendto() envía un datagrama directamente al broker.
     *
     * No es necesario establecer previamente una conexión.
     */
    int sent =
        sendto(
            subscriber_socket,
            subscription,
            (int)strlen(subscription),
            0,
            (struct sockaddr *)
                &broker_address,
            sizeof(broker_address)
        );


    if (sent == SOCKET_ERROR) {

        printf(
            "Error enviando suscripcion: %d\n",
            WSAGetLastError()
        );

        closesocket(
            subscriber_socket
        );

        WSACleanup();

        return 1;
    }


    printf(
        "Suscrito a [%s]. Esperando noticias...\n",
        topic
    );


    /*
     * Esperar datagramas enviados por el broker.
     */
    while (1) {

        char buffer[BUFFER_SIZE];

        struct sockaddr_in sender_address;

        int sender_length =
            sizeof(sender_address);


        int bytes_received =
            recvfrom(
                subscriber_socket,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *)
                    &sender_address,
                &sender_length
            );


        if (
            bytes_received ==
            SOCKET_ERROR
        ) {

            printf(
                "Error en recvfrom: %d\n",
                WSAGetLastError()
            );

            break;
        }


        buffer[bytes_received] =
            '\0';


        printf(
            "%s\n",
            buffer
        );
    }


    closesocket(
        subscriber_socket
    );

    WSACleanup();

    return 0;
}