# Laboratorio 3 - Análisis de capa de transporte y sockets

Implementación de un sistema publicación-suscripción usando sockets TCP y UDP en C.

## Estructura

- `tcp/`: implementación usando TCP
- `udp/`: implementación usando UDP
- `captures/`: capturas de tráfico de Wireshark
- `docs/`: evidencias y documentación

## Integrantes

- N Felipe Celis Díaz
- Juan Camilo Solano Estrada
- Antonio Muñoz Arcieri

## Documentación de funciones de librerías

- WSAStartup(MAKEWORD(2,2), &wsaData): Inicializa Winsock 2.2, que es la API de sockets de Windows. Debe ejecutarse antes de crear o utilizar cualquier socket; si retorna un valor distinto de 0, la inicialización falló.
- WSACleanup(): Finaliza el uso de Winsock y libera los recursos asociados a la biblioteca. Se ejecuta al cerrar normalmente el programa o cuando ocurre un error que obliga a terminarlo.
- WSAGetLastError(): Devuelve el código del último error generado por una función de Winsock. Se utiliza para diagnosticar fallos en operaciones como socket(), bind(), connect(), recv(), etc.
- socket(AF_INET, SOCK_STREAM/SOCK_DGRAM, IPPROTO_TCP/IPPROTO_UDP): Crea un nuevo socket de red. AF_INET indica IPv4; SOCK_STREAM se usa con TCP y SOCK_DGRAM con UDP; el tercer parámetro especifica explícitamente el protocolo.
- bind(socket, address, size): Asocia un socket con una dirección IP local y un puerto. En el laboratorio lo usa principalmente el broker para quedar disponible en un puerto conocido y recibir conexiones o datagramas.
- listen(socket, SOMAXCONN): Coloca un socket TCP en modo de escucha. A partir de ese momento el socket puede recibir solicitudes de conexión; SOMAXCONN permite usar el máximo backlog soportado por Winsock.
- accept(socket, address, address_size): Acepta una conexión TCP pendiente. Retorna un nuevo socket dedicado a la comunicación con ese cliente, mientras el socket original del broker continúa escuchando nuevas conexiones.
- connect(socket, address, size): Inicia una conexión TCP desde un cliente hacia el broker. En nuestro caso lo usan publishers y subscribers TCP para conectarse a 127.0.0.1:5000.
- send(socket, buffer, length, flags): Envía bytes a través de una conexión TCP ya establecida. Puede transmitir menos bytes que los solicitados, por lo que nuestra implementación usa send_all() para completar el envío.
- recv(socket, buffer, size, flags): Recibe bytes desde una conexión TCP. Devuelve la cantidad de bytes recibidos; 0 indica cierre ordenado de la conexión y SOCKET_ERROR indica un fallo.
- sendto(socket, buffer, length, flags, address, address_size): Envía un datagrama UDP directamente a una dirección IP y puerto específicos. No necesita una conexión previa como ocurre en TCP.
- recvfrom(socket, buffer, size, flags, address, address_size): Recibe un datagrama UDP y también entrega la dirección IP y el puerto del emisor. Esto permite al broker saber quién envió una suscripción o publicación.
- select(0, &read_set, NULL, NULL, NULL): Permite al broker TCP vigilar varios sockets al mismo tiempo dentro de un solo proceso. Se usa para detectar nuevas conexiones y datos disponibles sin bloquearse atendiendo únicamente a un cliente.
- FD_ZERO(&set): Inicializa o limpia un conjunto de sockets que será utilizado con select(). Debe ejecutarse antes de agregar sockets al conjunto.
- FD_SET(socket, &set): Agrega un socket al conjunto que será supervisado por select(). En el broker se agregan tanto el socket principal como los sockets de clientes conectados.
- FD_ISSET(socket, &set): Comprueba si un socket específico presentó actividad después de que select() retorna. Permite saber exactamente qué conexión debe ser atendida.
- closesocket(socket): Cierra un socket de Winsock y libera los recursos asociados a esa conexión. Se utiliza cuando termina un cliente o cuando se cierra el broker.
- htons(PORT): Convierte un número de puerto de 16 bits desde el orden de bytes del computador al orden de bytes utilizado por la red. Se aplica antes de almacenar el puerto en sockaddr_in.
- inet_pton(AF_INET, ip, &address): Convierte una dirección IPv4 escrita como texto, por ejemplo "127.0.0.1", a la representación binaria que utiliza internamente la estructura sockaddr_in.
- printf(format, ...): Imprime mensajes, estados y errores en la consola. Se utiliza ampliamente para mostrar el comportamiento del broker, publishers y subscribers durante las pruebas.
- fgets(buffer, size, stdin): Lee una línea ingresada por el usuario de forma controlada, respetando el tamaño máximo del buffer. Se usa para leer temas y eventos deportivos.
- strlen(string): Calcula la longitud de una cadena sin incluir el carácter final '\0'. Se usa, por ejemplo, para determinar cuántos bytes deben enviarse.
- strcmp(a, b): Compara dos cadenas completas. Retorna 0 cuando son iguales; la usamos, por ejemplo, para reconocer el comando "SALIR".
- strncmp(a, b, n): Compara únicamente los primeros n caracteres de dos cadenas. En el broker permite identificar si un mensaje comienza con SUB| o PUB|.
- strchr(string, character): Busca la primera aparición de un carácter dentro de una cadena. Se usa para encontrar el separador | entre el tema y el contenido de una publicación.
- strncpy(destination, source, n): Copia una cantidad máxima de caracteres entre cadenas. Se utiliza para guardar nombres de temas evitando sobrepasar el tamaño definido para cada buffer.
- strcspn(string, "\r\n"): Devuelve la posición del primer carácter \r o \n. Se usa para eliminar el salto de línea que fgets() normalmente deja al final de la entrada.
- snprintf(buffer, size, format, ...): Construye una cadena formateada respetando el tamaño máximo del buffer. Se utiliza para crear mensajes como PUB|tema|evento o MSG|tema|evento.
- memcpy(destination, source, n): Copia exactamente n bytes desde una región de memoria hacia otra. En TCP se usa para acumular datos recibidos dentro del buffer del cliente.
- memmove(destination, source, n): Mueve bytes incluso cuando las regiones de origen y destino se solapan. Se usa para desplazar al inicio del buffer los bytes TCP que todavía no han sido procesados.
- memchr(buffer, character, n): Busca un byte dentro de una región de memoria. En el broker TCP se usa para localizar '\n', que actúa como delimitador entre mensajes completos.
Funciones auxiliares implementadas por nosotros
- send_all(socket, data, length): Garantiza el envío completo de un mensaje TCP. Repite llamadas a send() hasta que todos los bytes hayan sido transmitidos o se produzca un error.
- initialize_clients(clients): Inicializa el arreglo de clientes TCP. Marca cada posición con INVALID_SOCKET y reinicia suscripciones y buffers para indicar que todavía no hay clientes registrados.
- find_free_client(clients): Recorre el arreglo de clientes TCP buscando una posición disponible. Retorna el índice libre o -1 si se alcanzó el máximo permitido.
- is_subscribed(client, topic): Comprueba si un cliente está suscrito a un tema determinado. Compara el tema solicitado con la lista de suscripciones almacenadas para ese cliente.
- subscribe_client(client, topic): Añade un nuevo tema a las suscripciones de un cliente. Evita duplicados y también impide superar el máximo de suscripciones configurado.
- distribute_message(...): Recorre los subscribers registrados y reenvía una publicación únicamente a aquellos que estén suscritos al tema correspondiente.
- process_message(...): Interpreta los mensajes del protocolo de aplicación. Identifica solicitudes SUB|tema y publicaciones PUB|tema|mensaje, y ejecuta la acción correspondiente.
- process_received_data(...): Maneja el flujo continuo de bytes de TCP. Acumula datos recibidos, detecta mensajes completos mediante \n y los envía a process_message().
- disconnect_client(clients, index): Cierra el socket de un cliente TCP que se desconectó o produjo un error y reinicia su posición para que pueda reutilizarse posteriormente.
- initialize_subscribers(subscribers): Inicializa el arreglo de subscribers UDP, marcándolos como inactivos y dejando su cantidad de suscripciones en cero.
- same_address(a, b): Compara dos direcciones UDP mediante IP y puerto. Permite determinar si dos datagramas provienen del mismo subscriber.
- find_subscriber(subscribers, address): Busca si una determinada combinación IP + puerto ya está registrada como subscriber UDP y retorna su índice si existe.
- find_free_subscriber(subscribers): Busca una posición disponible dentro del arreglo de subscribers UDP. Retorna el índice libre o -1 cuando se alcanza el límite configurado.