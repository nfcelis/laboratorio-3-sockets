#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 5001

#define MAX_SUBSCRIBERS 20
#define MAX_SUBSCRIPTIONS 10
#define MAX_TOPIC_LENGTH 50
#define BUFFER_SIZE 1024


/*
 * Representa un subscriber UDP.
 *
 * A diferencia de TCP, no existe un socket específico por cliente.
 * Por eso debemos almacenar explícitamente su dirección IP y puerto.
 */
typedef struct {

    struct sockaddr_in address;

    char topics[MAX_SUBSCRIPTIONS][MAX_TOPIC_LENGTH];

    int topic_count;

    int active;

} Subscriber;


/*
 * Inicializa la lista de subscribers.
 */
void initialize_subscribers(Subscriber subscribers[]) {

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {

        subscribers[i].active = 0;
        subscribers[i].topic_count = 0;
    }
}


/*
 * Compara dos direcciones UDP.
 *
 * Dos clientes se consideran iguales si tienen:
 *
 * - misma dirección IP
 * - mismo puerto
 */
int same_address(
    struct sockaddr_in *a,
    struct sockaddr_in *b
) {

    return (
        a->sin_addr.s_addr == b->sin_addr.s_addr &&
        a->sin_port == b->sin_port
    );
}


/*
 * Busca un subscriber previamente registrado.
 *
 * Retorna:
 * índice si existe.
 * -1 si no existe.
 */
int find_subscriber(
    Subscriber subscribers[],
    struct sockaddr_in *address
) {

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {

        if (
            subscribers[i].active &&
            same_address(
                &subscribers[i].address,
                address
            )
        ) {

            return i;
        }
    }

    return -1;
}


/*
 * Busca una posición libre.
 */
int find_free_subscriber(
    Subscriber subscribers[]
) {

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {

        if (!subscribers[i].active) {
            return i;
        }
    }

    return -1;
}


/*
 * Verifica si un subscriber está suscrito a un tema.
 */
int is_subscribed(
    Subscriber *subscriber,
    const char *topic
) {

    for (int i = 0; i < subscriber->topic_count; i++) {

        if (
            strcmp(
                subscriber->topics[i],
                topic
            ) == 0
        ) {

            return 1;
        }
    }

    return 0;
}


/*
 * Registra una suscripción.
 */
void subscribe_client(
    Subscriber *subscriber,
    const char *topic
) {

    if (is_subscribed(subscriber, topic)) {
        return;
    }

    if (
        subscriber->topic_count >=
        MAX_SUBSCRIPTIONS
    ) {

        printf(
            "Subscriber alcanzo el limite de suscripciones.\n"
        );

        return;
    }

    strncpy(
        subscriber->topics[
            subscriber->topic_count
        ],
        topic,
        MAX_TOPIC_LENGTH - 1
    );

    subscriber->topics[
        subscriber->topic_count
    ][MAX_TOPIC_LENGTH - 1] = '\0';

    subscriber->topic_count++;
}


/*
 * Envía una publicación a todos los subscribers
 * suscritos al tema correspondiente.
 *
 * En UDP usamos sendto() porque no existe una conexión
 * persistente con cada subscriber.
 */
void distribute_message(
    SOCKET broker_socket,
    Subscriber subscribers[],
    const char *topic,
    const char *message
) {

    char outgoing[BUFFER_SIZE];

    snprintf(
        outgoing,
        sizeof(outgoing),
        "MSG|%s|%s",
        topic,
        message
    );

    printf(
        "Distribuyendo [%s]: %s\n",
        topic,
        message
    );

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {

        if (
            subscribers[i].active &&
            is_subscribed(
                &subscribers[i],
                topic
            )
        ) {

            int result = sendto(
                broker_socket,
                outgoing,
                (int)strlen(outgoing),
                0,
                (struct sockaddr *)
                    &subscribers[i].address,
                sizeof(
                    subscribers[i].address
                )
            );


            if (result == SOCKET_ERROR) {

                printf(
                    "Error enviando a subscriber %d: %d\n",
                    i,
                    WSAGetLastError()
                );
            }
        }
    }
}


/*
 * Procesa un datagrama UDP recibido.
 */
void process_message(
    SOCKET broker_socket,
    Subscriber subscribers[],
    struct sockaddr_in *client_address,
    char *message
) {

    /*
     * Suscripción:
     *
     * SUB|tema
     */
    if (
        strncmp(
            message,
            "SUB|",
            4
        ) == 0
    ) {

        char *topic =
            message + 4;


        if (strlen(topic) == 0) {

            printf(
                "SUB recibido sin tema.\n"
            );

            return;
        }


        int index =
            find_subscriber(
                subscribers,
                client_address
            );


        /*
         * Si el subscriber todavía no existe,
         * se registra usando IP + puerto.
         */
        if (index == -1) {

            index =
                find_free_subscriber(
                    subscribers
                );


            if (index == -1) {

                printf(
                    "Limite de subscribers alcanzado.\n"
                );

                return;
            }


            subscribers[index].address =
                *client_address;

            subscribers[index].active = 1;
            subscribers[index].topic_count = 0;
        }


        subscribe_client(
            &subscribers[index],
            topic
        );


        printf(
            "Subscriber %d suscrito a: %s\n",
            index,
            topic
        );


        return;
    }


    /*
     * Publicación:
     *
     * PUB|tema|mensaje
     */
    if (
        strncmp(
            message,
            "PUB|",
            4
        ) == 0
    ) {

        char *topic =
            message + 4;


        char *separator =
            strchr(
                topic,
                '|'
            );


        if (separator == NULL) {

            printf(
                "Formato PUB invalido.\n"
            );

            return;
        }


        *separator = '\0';


        char *content =
            separator + 1;


        if (
            strlen(topic) == 0 ||
            strlen(content) == 0
        ) {

            printf(
                "PUB recibido con campos vacios.\n"
            );

            return;
        }


        printf(
            "Publicacion recibida.\n"
        );


        distribute_message(
            broker_socket,
            subscribers,
            topic,
            content
        );


        return;
    }


    printf(
        "Mensaje UDP desconocido: %s\n",
        message
    );
}


int main() {

    WSADATA wsaData;

    SOCKET broker_socket;

    struct sockaddr_in broker_address;

    Subscriber subscribers[MAX_SUBSCRIBERS];


    printf(
        "=== BROKER UDP ===\n"
    );


    /*
     * Inicializa Winsock 2.2.
     */
    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0
    ) {

        printf(
            "Error inicializando Winsock: %d\n",
            WSAGetLastError()
        );

        return 1;
    }


    initialize_subscribers(
        subscribers
    );


    /*
     * Crea un socket UDP IPv4.
     *
     * SOCK_DGRAM:
     * socket orientado a datagramas.
     *
     * IPPROTO_UDP:
     * protocolo UDP.
     */
    broker_socket = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP
    );


    if (
        broker_socket ==
        INVALID_SOCKET
    ) {

        printf(
            "Error creando socket UDP: %d\n",
            WSAGetLastError()
        );

        WSACleanup();

        return 1;
    }


    /*
     * Configura la dirección local del broker.
     */
    broker_address.sin_family =
        AF_INET;

    broker_address.sin_addr.s_addr =
        INADDR_ANY;

    broker_address.sin_port =
        htons(PORT);


    /*
     * Asocia el socket UDP al puerto 5001.
     */
    if (
        bind(
            broker_socket,
            (struct sockaddr *)
                &broker_address,
            sizeof(broker_address)
        ) == SOCKET_ERROR
    ) {

        printf(
            "Error en bind: %d\n",
            WSAGetLastError()
        );

        closesocket(
            broker_socket
        );

        WSACleanup();

        return 1;
    }


    printf(
        "Broker UDP escuchando en puerto %d...\n",
        PORT
    );


    /*
     * Bucle principal.
     *
     * UDP no requiere accept().
     *
     * recvfrom() recibe un datagrama junto con la
     * dirección IP y puerto de quien lo envió.
     */
    while (1) {

        char buffer[BUFFER_SIZE];

        struct sockaddr_in client_address;

        int client_address_length =
            sizeof(client_address);


        int bytes_received =
            recvfrom(
                broker_socket,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *)
                    &client_address,
                &client_address_length
            );


        if (
            bytes_received ==
            SOCKET_ERROR
        ) {

            printf(
                "Error en recvfrom: %d\n",
                WSAGetLastError()
            );

            continue;
        }


        /*
         * Añadir terminador para tratar los bytes
         * recibidos como una cadena de C.
         */
        buffer[bytes_received] =
            '\0';


        process_message(
            broker_socket,
            subscribers,
            &client_address,
            buffer
        );
    }


    closesocket(
        broker_socket
    );

    WSACleanup();

    return 0;
}