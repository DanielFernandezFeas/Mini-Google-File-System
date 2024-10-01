#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <pthread.h>
#include <string.h>
#include "common_srv.h"
#include "common_cln.h"

// Información que se la pasa al thread creado
typedef struct thread_info {
    int socket; // Añadir los campos necesarios
} thread_info;

// Función del thread
void *servicio(void *arg) {
    thread_info *thinf = arg;
    // Aquí se manejarían las peticiones de escritura en fases posteriores
    // Por ahora, simplemente cerramos la conexión
    close(thinf->socket);
    free(thinf);
    return NULL;
}

int main(int argc, char *argv[]) {
    int s, s_conec;
    unsigned int tam_dir;
    struct sockaddr_in dir_cliente;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s nombre_dir master_host master_puerto\n", argv[0]);
        return -1;
    }

    // Asegurarse de que el directorio de almacenamiento existe
    mkdir(argv[1], 0755);

    // Crear socket de servidor
    int server_socket, port;
    if ((server_socket = create_socket_srv(0, &port)) < 0) {
        perror("Error al crear el socket del servidor");
        return -1;
    }
    printf("Socket del servidor creado en el puerto: %d\n", port);

    // Conectarse al maestro para registrarse
    int master_socket = create_socket_cln_by_name(argv[2], argv[3]);
    if (master_socket < 0) {
        perror("Error al conectar con el maestro");
        return -1;
    }
    printf("Conectado al maestro en %s:%s\n", argv[2], argv[3]);

    // Enviar el puerto al maestro
    char op_code = 'P'; // Código de operación para registrar servidor
    unsigned short port_net = htons(port);
    printf("Registrando servidor en puerto %d (net: %d)\n", port, port_net);
    struct iovec iov[2];
    iov[0].iov_base = &op_code;
    iov[0].iov_len = sizeof(op_code);
    iov[1].iov_base = &port_net;
    iov[1].iov_len = sizeof(port_net);

    if (writev(master_socket, iov, 2) < 0) {
        perror("Error al enviar datos al maestro");
        close(master_socket);
        return -1;
    }
    printf("Datos enviados al maestro: Código de operación '%c', Puerto %d (net: %d)\n", op_code, port, port_net);
    close(master_socket);

    // Preparar atributos adecuados para crear thread "detached"
    pthread_t thid;
    pthread_attr_t atrib_th;
    pthread_attr_init(&atrib_th); // Evita pthread_join
    pthread_attr_setdetachstate(&atrib_th, PTHREAD_CREATE_DETACHED);

    while (1) {
        tam_dir = sizeof(dir_cliente);
        // Acepta la conexión
        if ((s_conec = accept(server_socket, (struct sockaddr *)&dir_cliente, &tam_dir)) < 0) {
            perror("Error en accept");
            close(server_socket);
            return -1;
        }
        printf("Conectado cliente con ip %s y puerto %d\n", inet_ntoa(dir_cliente.sin_addr), ntohs(dir_cliente.sin_port));
        // Crea el thread de servicio
        thread_info *thinf = malloc(sizeof(thread_info));
        if (!thinf) {
            perror("Error al asignar memoria para thread_info");
            close(s_conec);
            continue;
        }
        thinf->socket = s_conec;
        if (pthread_create(&thid, &atrib_th, servicio, thinf) != 0) {
            perror("Error al crear el thread de servicio");
            close(s_conec);
            free(thinf);
        }
    }

    close(server_socket); // Cierra el socket general
    return 0;
}
