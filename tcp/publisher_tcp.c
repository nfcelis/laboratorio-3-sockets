#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000

#define BUFFER_SIZE 1024


/*
 * Garantiza que todos los bytes sean enviados mediante TCP.
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

    SOCKET publisher_socket;

    struct sockaddr_in broker_address;

    char topic[100];

    char event[700];

    char outgoing[BUFFER_SIZE];


    printf("=== PUBLISHER TCP ===\n");


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
    publisher_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );


    if (publisher_socket == INVALID_SOCKET) {

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


    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &broker_address.sin_addr
        ) != 1) {

        printf(
            "Direccion IP invalida.\n"
        );

        closesocket(publisher_socket);

        WSACleanup();

        return 1;
    }


    /*
     * Conectarse al broker.
     */
    if (connect(
            publisher_socket,
            (struct sockaddr *)&broker_address,
            sizeof(broker_address)
        ) == SOCKET_ERROR) {

        printf(
            "Error conectando al broker: %d\n",
            WSAGetLastError()
        );

        closesocket(publisher_socket);

        WSACleanup();

        return 1;
    }


    printf(
        "Conectado al broker %s:%d\n",
        SERVER_IP,
        PORT
    );


    /*
     * El publisher representa un periodista cubriendo
     * un partido específico.
     */
    printf(
        "Partido que esta cubriendo: "
    );

    fgets(
        topic,
        sizeof(topic),
        stdin
    );

    topic[strcspn(topic, "\r\n")] = '\0';


    printf(
        "\nIngrese los eventos del partido.\n"
    );

    printf(
        "Escriba SALIR para terminar.\n\n"
    );


    while (1) {

        printf(
            "Evento: "
        );


        fgets(
            event,
            sizeof(event),
            stdin
        );


        event[strcspn(event, "\r\n")] = '\0';


        if (strcmp(
                event,
                "SALIR"
            ) == 0) {

            break;
        }


        /*
         * Construir mensaje:
         *
         * PUB|PARTIDO|EVENTO\n
         */
        snprintf(
            outgoing,
            sizeof(outgoing),
            "PUB|%s|%s\n",
            topic,
            event
        );


        if (send_all(
                publisher_socket,
                outgoing,
                (int)strlen(outgoing)
            ) == -1) {

            printf(
                "Error enviando mensaje.\n"
            );

            break;
        }


        printf(
            "Evento enviado.\n"
        );
    }


    closesocket(publisher_socket);

    WSACleanup();

    return 0;
}