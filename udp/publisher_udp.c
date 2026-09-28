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

    SOCKET publisher_socket;

    struct sockaddr_in broker_address;

    char topic[100];

    char event[700];

    char outgoing[BUFFER_SIZE];


    printf(
        "=== PUBLISHER UDP ===\n"
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
     * Crear socket UDP IPv4.
     */
    publisher_socket = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP
    );


    if (
        publisher_socket ==
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
     * Dirección del broker.
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
            publisher_socket
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
        "Partido que esta cubriendo: "
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


        event[
            strcspn(
                event,
                "\r\n"
            )
        ] = '\0';


        if (
            strcmp(
                event,
                "SALIR"
            ) == 0
        ) {

            break;
        }


        /*
         * Construir datagrama:
         *
         * PUB|PARTIDO|EVENTO
         */
        snprintf(
            outgoing,
            sizeof(outgoing),
            "PUB|%s|%s",
            topic,
            event
        );


        int sent =
            sendto(
                publisher_socket,
                outgoing,
                (int)strlen(outgoing),
                0,
                (struct sockaddr *)
                    &broker_address,
                sizeof(broker_address)
            );


        if (
            sent ==
            SOCKET_ERROR
        ) {

            printf(
                "Error enviando evento: %d\n",
                WSAGetLastError()
            );

            break;
        }


        printf(
            "Evento enviado.\n"
        );
    }


    closesocket(
        publisher_socket
    );

    WSACleanup();

    return 0;
}