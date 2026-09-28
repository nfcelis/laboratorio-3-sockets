#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 5000

#define MAX_CLIENTS 20
#define MAX_SUBSCRIPTIONS 10
#define MAX_TOPIC_LENGTH 50
#define BUFFER_SIZE 1024

/*
 * Estructura utilizada para almacenar la información asociada
 * a cada cliente conectado al broker.
 *
 * socket:
 *      socket TCP específico de ese cliente.
 *
 * topics:
 *      lista de partidos/temas a los cuales se encuentra suscrito.
 *
 * topic_count:
 *      cantidad actual de suscripciones.
 *
 * recv_buffer:
 *      buffer utilizado para reconstruir mensajes TCP.
 *
 * recv_length:
 *      cantidad de bytes actualmente almacenados en recv_buffer.
 */
typedef struct {

    SOCKET socket;

    char topics[MAX_SUBSCRIPTIONS][MAX_TOPIC_LENGTH];
    int topic_count;

    char recv_buffer[BUFFER_SIZE * 2];
    int recv_length;

} Client;


/*
 * Inicializa todas las posiciones del arreglo de clientes.
 *
 * INVALID_SOCKET indica que esa posición todavía no contiene
 * un cliente conectado.
 */
void initialize_clients(Client clients[]) {

    for (int i = 0; i < MAX_CLIENTS; i++) {

        clients[i].socket = INVALID_SOCKET;
        clients[i].topic_count = 0;
        clients[i].recv_length = 0;
    }
}


/*
 * Busca una posición libre dentro del arreglo de clientes.
 *
 * Retorna:
 *      índice disponible si existe.
 *      -1 si se alcanzó MAX_CLIENTS.
 */
int find_free_client(Client clients[]) {

    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (clients[i].socket == INVALID_SOCKET) {
            return i;
        }
    }

    return -1;
}


/*
 * Verifica si un cliente se encuentra suscrito a un tema.
 *
 * Retorna:
 *      1 si está suscrito.
 *      0 si no está suscrito.
 */
int is_subscribed(Client *client, const char *topic) {

    for (int i = 0; i < client->topic_count; i++) {

        if (strcmp(client->topics[i], topic) == 0) {
            return 1;
        }
    }

    return 0;
}


/*
 * Registra una nueva suscripción.
 *
 * Evita:
 *      - suscripciones duplicadas.
 *      - superar MAX_SUBSCRIPTIONS.
 */
void subscribe_client(Client *client, const char *topic) {

    if (is_subscribed(client, topic)) {
        return;
    }

    if (client->topic_count >= MAX_SUBSCRIPTIONS) {
        printf("Cliente alcanzo el limite de suscripciones.\n");
        return;
    }

    strncpy(
        client->topics[client->topic_count],
        topic,
        MAX_TOPIC_LENGTH - 1
    );

    client->topics[client->topic_count][MAX_TOPIC_LENGTH - 1] = '\0';

    client->topic_count++;
}


/*
 * TCP trabaja como un flujo de bytes.
 *
 * send() puede enviar menos bytes que los solicitados.
 * Esta función repite send() hasta transmitir el mensaje completo.
 *
 * Retorna:
 *      0 si todo fue enviado.
 *      -1 si ocurrió un error.
 */
int send_all(SOCKET socket, const char *data, int length) {

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


/*
 * Distribuye un mensaje únicamente a los clientes que estén
 * suscritos al tema correspondiente.
 */
void distribute_message(
    Client clients[],
    const char *topic,
    const char *message
) {

    char outgoing[BUFFER_SIZE];

    snprintf(
        outgoing,
        sizeof(outgoing),
        "MSG|%s|%s\n",
        topic,
        message
    );

    printf(
        "Distribuyendo [%s]: %s\n",
        topic,
        message
    );

    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (clients[i].socket != INVALID_SOCKET &&
            is_subscribed(&clients[i], topic)) {

            if (send_all(
                    clients[i].socket,
                    outgoing,
                    (int)strlen(outgoing)
                ) == -1) {

                printf(
                    "Error enviando mensaje a cliente %d.\n",
                    i
                );
            }
        }
    }
}


/*
 * Procesa una línea completa recibida por TCP.
 *
 * Formatos reconocidos:
 *
 * SUB|tema
 *
 * PUB|tema|mensaje
 */
void process_message(
    Client clients[],
    int client_index,
    char *message
) {

    Client *client = &clients[client_index];

    /*
     * Mensaje de suscripción.
     */
    if (strncmp(message, "SUB|", 4) == 0) {

        char *topic = message + 4;

        if (strlen(topic) == 0) {
            printf("SUB recibido sin tema.\n");
            return;
        }

        subscribe_client(client, topic);

        printf(
            "Cliente %d suscrito a: %s\n",
            client_index,
            topic
        );

        return;
    }


    /*
     * Mensaje de publicación.
     *
     * Esperamos:
     *
     * PUB|tema|mensaje
     */
    if (strncmp(message, "PUB|", 4) == 0) {

        char *topic = message + 4;

        /*
         * Busca el segundo separador '|'.
         */
        char *separator = strchr(topic, '|');

        if (separator == NULL) {
            printf("Formato PUB invalido.\n");
            return;
        }

        /*
         * Sustituimos temporalmente el separador por '\0'
         * para obtener dos cadenas independientes:
         *
         * topic
         * content
         */
        *separator = '\0';

        char *content = separator + 1;

        if (strlen(topic) == 0 ||
            strlen(content) == 0) {

            printf("PUB recibido con campos vacios.\n");
            return;
        }

        printf(
            "Publicacion recibida de cliente %d.\n",
            client_index
        );

        distribute_message(
            clients,
            topic,
            content
        );

        return;
    }


    printf(
        "Mensaje desconocido de cliente %d: %s\n",
        client_index,
        message
    );
}


/*
 * TCP no conserva fronteras de mensajes.
 *
 * Un recv() podría recibir:
 *
 * 1. Un mensaje completo.
 * 2. Medio mensaje.
 * 3. Varios mensajes juntos.
 *
 * Por eso utilizamos '\n' como delimitador de mensajes.
 *
 * Esta función acumula los bytes recibidos y procesa únicamente
 * las líneas completas.
 */
void process_received_data(
    Client clients[],
    int client_index,
    const char *data,
    int length
) {

    Client *client = &clients[client_index];

    /*
     * Evita desbordar el buffer.
     */
    if (client->recv_length + length >= sizeof(client->recv_buffer)) {

        printf(
            "Buffer excedido para cliente %d. Reiniciando buffer.\n",
            client_index
        );

        client->recv_length = 0;
        return;
    }


    memcpy(
        client->recv_buffer + client->recv_length,
        data,
        length
    );

    client->recv_length += length;


    while (1) {

        char *newline = memchr(
            client->recv_buffer,
            '\n',
            client->recv_length
        );

        /*
         * No existe todavía una línea completa.
         */
        if (newline == NULL) {
            break;
        }

        int message_length =
            (int)(newline - client->recv_buffer);

        /*
         * Elimina '\r' cuando el mensaje viene como "\r\n".
         */
        if (message_length > 0 &&
            client->recv_buffer[message_length - 1] == '\r') {

            message_length--;
        }


        char message[BUFFER_SIZE];

        if (message_length >= BUFFER_SIZE) {
            message_length = BUFFER_SIZE - 1;
        }


        memcpy(
            message,
            client->recv_buffer,
            message_length
        );

        message[message_length] = '\0';


        /*
         * Cantidad de bytes consumidos incluyendo '\n'.
         */
        int consumed =
            (int)((newline - client->recv_buffer) + 1);


        /*
         * Desplaza los bytes restantes al comienzo del buffer.
         */
        memmove(
            client->recv_buffer,
            client->recv_buffer + consumed,
            client->recv_length - consumed
        );

        client->recv_length -= consumed;


        process_message(
            clients,
            client_index,
            message
        );
    }
}


/*
 * Cierra la conexión con un cliente y libera su posición
 * en el arreglo.
 */
void disconnect_client(
    Client clients[],
    int index
) {

    printf(
        "Cliente %d desconectado.\n",
        index
    );

    closesocket(clients[index].socket);

    clients[index].socket = INVALID_SOCKET;
    clients[index].topic_count = 0;
    clients[index].recv_length = 0;
}


int main() {

    WSADATA wsaData;

    SOCKET broker_socket;

    struct sockaddr_in broker_address;

    Client clients[MAX_CLIENTS];


    printf("=== BROKER TCP ===\n");


    /*
     * Inicializa Winsock 2.2.
     */
    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0) {

        printf(
            "Error inicializando Winsock: %d\n",
            WSAGetLastError()
        );

        return 1;
    }


    /*
     * Inicializa el arreglo donde almacenaremos los clientes.
     */
    initialize_clients(clients);


    /*
     * Crea socket TCP IPv4.
     */
    broker_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (broker_socket == INVALID_SOCKET) {

        printf(
            "Error creando socket: %d\n",
            WSAGetLastError()
        );

        WSACleanup();

        return 1;
    }


    /*
     * Configuración de dirección del broker.
     */
    broker_address.sin_family = AF_INET;

    broker_address.sin_addr.s_addr =
        INADDR_ANY;

    broker_address.sin_port =
        htons(PORT);


    /*
     * Asocia el socket al puerto 5000.
     */
    if (bind(
            broker_socket,
            (struct sockaddr *)&broker_address,
            sizeof(broker_address)
        ) == SOCKET_ERROR) {

        printf(
            "Error en bind: %d\n",
            WSAGetLastError()
        );

        closesocket(broker_socket);
        WSACleanup();

        return 1;
    }


    /*
     * Coloca el socket en modo escucha.
     */
    if (listen(
            broker_socket,
            SOMAXCONN
        ) == SOCKET_ERROR) {

        printf(
            "Error en listen: %d\n",
            WSAGetLastError()
        );

        closesocket(broker_socket);
        WSACleanup();

        return 1;
    }


    printf(
        "Broker escuchando en puerto %d...\n",
        PORT
    );


    /*
     * Bucle principal del broker.
     *
     * select() permite esperar actividad simultáneamente:
     *
     * - nuevas conexiones;
     * - datos de clientes existentes.
     */
    while (1) {

        fd_set read_set;

        FD_ZERO(&read_set);


        /*
         * Vigilar el socket principal.
         *
         * Cuando tenga actividad significa que existe
         * una conexión TCP pendiente.
         */
        FD_SET(
            broker_socket,
            &read_set
        );


        /*
         * Agrega todos los sockets de clientes conectados.
         */
        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (clients[i].socket != INVALID_SOCKET) {

                FD_SET(
                    clients[i].socket,
                    &read_set
                );
            }
        }


        /*
         * En Winsock el primer parámetro de select() se ignora,
         * por eso utilizamos 0.
         *
         * NULL como timeout significa esperar indefinidamente
         * hasta que ocurra algún evento.
         */
        int activity = select(
            0,
            &read_set,
            NULL,
            NULL,
            NULL
        );


        if (activity == SOCKET_ERROR) {

            printf(
                "Error en select: %d\n",
                WSAGetLastError()
            );

            break;
        }


        /*
         * Si broker_socket está activo existe una nueva conexión.
         */
        if (FD_ISSET(
                broker_socket,
                &read_set
            )) {

            struct sockaddr_in client_address;

            int client_address_length =
                sizeof(client_address);


            /*
             * accept() crea un nuevo socket específico
             * para comunicarse con ese cliente.
             */
            SOCKET client_socket = accept(
                broker_socket,
                (struct sockaddr *)&client_address,
                &client_address_length
            );


            if (client_socket == INVALID_SOCKET) {

                printf(
                    "Error en accept: %d\n",
                    WSAGetLastError()
                );

            } else {

                int index =
                    find_free_client(clients);


                if (index == -1) {

                    printf(
                        "Limite de clientes alcanzado.\n"
                    );

                    closesocket(client_socket);

                } else {

                    clients[index].socket =
                        client_socket;

                    clients[index].topic_count = 0;
                    clients[index].recv_length = 0;


                    printf(
                        "Nuevo cliente conectado. ID interno: %d\n",
                        index
                    );
                }
            }
        }


        /*
         * Revisar cuáles clientes tienen datos disponibles.
         */
        for (int i = 0; i < MAX_CLIENTS; i++) {

            SOCKET current_socket =
                clients[i].socket;


            if (current_socket == INVALID_SOCKET) {
                continue;
            }


            if (FD_ISSET(
                    current_socket,
                    &read_set
                )) {

                char buffer[BUFFER_SIZE];


                /*
                 * recv() lee los bytes disponibles en la
                 * conexión TCP.
                 */
                int bytes_received = recv(
                    current_socket,
                    buffer,
                    sizeof(buffer),
                    0
                );


                /*
                 * recv() == 0 significa que el otro extremo
                 * cerró ordenadamente la conexión.
                 */
                if (bytes_received == 0) {

                    disconnect_client(
                        clients,
                        i
                    );

                }

                /*
                 * SOCKET_ERROR indica error durante recepción.
                 */
                else if (
                    bytes_received == SOCKET_ERROR
                ) {

                    printf(
                        "Error recibiendo datos del cliente %d: %d\n",
                        i,
                        WSAGetLastError()
                    );

                    disconnect_client(
                        clients,
                        i
                    );

                }

                /*
                 * Datos recibidos correctamente.
                 */
                else {

                    process_received_data(
                        clients,
                        i,
                        buffer,
                        bytes_received
                    );
                }
            }
        }
    }


    /*
     * Cierre ordenado del broker.
     */
    for (int i = 0; i < MAX_CLIENTS; i++) {

        if (clients[i].socket != INVALID_SOCKET) {

            closesocket(
                clients[i].socket
            );
        }
    }


    closesocket(broker_socket);

    WSACleanup();

    return 0;
}