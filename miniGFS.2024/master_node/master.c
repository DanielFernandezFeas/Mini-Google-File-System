// EJEMPLO DE SERVIDOR MULTITHREAD QUE RECIBE PETICIONES DE LOS CLIENTES.
// PUEDE USARLO COMO BASE PARA DESARROLLAR EL MASTER Y EL SERVER DE LA PRÁCTICA.
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <pthread.h>
#include "common_srv.h"
#include "map.h"
#include "array.h"
#include <string.h>
#include <arpa/inet.h> // para inet_ntoa, inet_ntop e inet_pton


typedef struct {
    int blocksize;
    int rep_factor;
    const char *fname;
} file_info;

typedef struct server_info {
    char ip[INET_ADDRSTRLEN]; // Dirección IP
    int port;                 // Puerto
} server_info;
// Declaración del mapa para almacenar los ficheros
static map *file_map;

static array *server_list;

// Función de inicialización del maestro
void init_master() {
    file_map = map_create(key_string, 0); // Crear el mapa con cerrojos
    server_list = array_create(0); // Crear el array sin cerrojos
}

// información que se la pasa el thread creado
typedef struct thread_info {
    int socket; // añadir los campos necesarios
} thread_info;

// función del thread
void *servicio(void *arg) {
    thread_info *thinf = arg;
    char op_code;

    while (1) {
        // Recibir el código de operación
        if (recv(thinf->socket, &op_code, sizeof(op_code), MSG_WAITALL) != sizeof(op_code)) {
            perror("Error al recibir el código de operación");
            break;
        }
        printf("Operación recibida: %c\n", op_code);

        switch (op_code) {
            case 'C': {  // Operación de crear fichero
                int blocksize, rep_factor;
                int fname_len;
                const char *fname;  // Usar puntero directo

                // Recibir el tamaño del bloque
                if (recv(thinf->socket, &blocksize, sizeof(blocksize), MSG_WAITALL) != sizeof(blocksize)) {
                    perror("Error al recibir blocksize");
                    break;
                }
                blocksize = ntohl(blocksize);

                // Recibir el factor de replicación
                if (recv(thinf->socket, &rep_factor, sizeof(rep_factor), MSG_WAITALL) != sizeof(rep_factor)) {
                    perror("Error al recibir rep_factor");
                    break;
                }
                rep_factor = ntohl(rep_factor);

                // Recibir la longitud del nombre del fichero
                if (recv(thinf->socket, &fname_len, sizeof(fname_len), MSG_WAITALL) != sizeof(fname_len)) {
                    perror("Error al recibir fname_len");
                    break;
                }
                fname_len = ntohl(fname_len);

                // Reservar memoria para el nombre del fichero
                fname = malloc(fname_len + 1);
                if (!fname) {
                    perror("Error al asignar memoria para fname");
                    break;
                }

                // Recibir el nombre del fichero
                if (recv(thinf->socket, (void *)fname, fname_len, MSG_WAITALL) != fname_len) {
                    perror("Error al recibir fname");
                    free((void *)fname);  // Convertir a puntero no constante para liberar memoria
                    break;
                }
                ((char *)fname)[fname_len] = '\0'; // Añadir terminador nulo

                // Crear el descriptor del fichero
                file_info *finfo = malloc(sizeof(file_info));
                if (!finfo) {
                    perror("Error al asignar memoria para file_info");
                    free((void *)fname);  // Convertir a puntero no constante para liberar memoria
                    int error = -1;
                    error = htonl(error);
                    write(thinf->socket, &error, sizeof(error));
                    break;
                }

                finfo->blocksize = blocksize;
                finfo->rep_factor = rep_factor;
                finfo->fname = fname;  // Asignar directamente sin duplicar

                // Añadir el fichero al mapa
                if (map_put(file_map, finfo->fname, finfo) < 0) {
                    perror("Error al añadir el fichero al mapa");
                    free((void *)finfo->fname);  // Convertir a puntero no constante para liberar memoria
                    free(finfo);
                    int error = -1;
                    error = htonl(error);
                    write(thinf->socket, &error, sizeof(error));
                    break;
                }

                // Enviar respuesta de éxito
                int success = 0;
                success = htonl(success);
                if (write(thinf->socket, &success, sizeof(success)) < 0) {
                    perror("Error al enviar respuesta de éxito");
                }
                break;
            }
            case 'N': {  // Operación para obtener el número de ficheros
                printf("Procesando la operación 'N'\n");
                int n_files = map_size(file_map);
                n_files = htonl(n_files); // Convertir a formato de red
                if (write(thinf->socket, &n_files, sizeof(n_files)) < 0) {
                    perror("Error al enviar el número de ficheros");
                } else {
                    perror("Enviado el número de ficheros");
                }
                break;
            }
            case 'O': {  // Operación de abrir fichero
                int fname_len;
                char fname[256];

                // Recibir la longitud del nombre del fichero
                if (recv(thinf->socket, &fname_len, sizeof(fname_len), MSG_WAITALL) != sizeof(fname_len)) {
                    perror("Error al recibir fname_len");
                    break;
                }
                fname_len = ntohl(fname_len);

                // Recibir el nombre del fichero
                if (recv(thinf->socket, fname, fname_len, MSG_WAITALL) != fname_len) {
                    perror("Error al recibir fname");
                    break;
                }
                fname[fname_len] = '\0';

                // Buscar el fichero en el mapa
                int err;
                file_info *finfo = map_get(file_map, fname, &err);
                if (err == -1) {
                    perror("Fichero no encontrado");
                    int error = -1;
                    error = htonl(error);
                    if (write(thinf->socket, &error, sizeof(error)) < 0) {
                        perror("Error al enviar el error al cliente");
                    }
                    break;
                }

                // Enviar el tamaño del bloque y el factor de replicación
                int blocksize_net = htonl(finfo->blocksize);
                int rep_factor_net = htonl(finfo->rep_factor);

                struct iovec iov[2];
                iov[0].iov_base = &blocksize_net;
                iov[0].iov_len = sizeof(blocksize_net);
                iov[1].iov_base = &rep_factor_net;
                iov[1].iov_len = sizeof(rep_factor_net);

                if (writev(thinf->socket, iov, 2) < 0) {
                    perror("Error al enviar datos del fichero");
                }
                break;
            }
                case 'P': { // Registrar información del servidor
                    unsigned short port_net;
    if (recv(thinf->socket, &port_net, sizeof(port_net), MSG_WAITALL) != sizeof(port_net)) {
        perror("Error al recibir puerto");
        break;
    }
    unsigned short port = ntohs(port_net);

    // Obtener la dirección IP del servidor
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(thinf->socket, (struct sockaddr *)&addr, &addr_len) == -1) {
        perror("Error al obtener la dirección IP del servidor");
        break;
    }

    server_info *sinfo = malloc(sizeof(server_info));
    if (!sinfo) {
        perror("Error al asignar memoria para server_info");
        break;
    }
    inet_ntop(AF_INET, &addr.sin_addr, sinfo->ip, sizeof(sinfo->ip));

    // Imprimir la dirección IP y el puerto del servidor registrado
    printf("Registrando servidor con IP texto: %s y puerto %d\n",
           sinfo->ip, port);

    sinfo->port = port;
    if (array_append(server_list, sinfo) == -1) {
        perror("Error al añadir el servidor al array");
        free(sinfo);
        break;
    }
    printf("Servidor registrado con IP %s y puerto %d\n", sinfo->ip, port);
    break;
}

case 'S': { // Solicitar información del servidor
    int server_num_net;
    if (recv(thinf->socket, &server_num_net, sizeof(server_num_net), MSG_WAITALL) != sizeof(server_num_net)) {
        perror("Error al recibir número de servidor");
        break;
    }
    int server_num = ntohl(server_num_net);

    int error;
    server_info *sinfo = array_get(server_list, server_num, &error);
    if (error) {
        perror("Error al acceder a la información del servidor");
        int error_code = htonl(-1);
        write(thinf->socket, &error_code, sizeof(error_code));
        break;
    }

    struct {
        unsigned int ip_net;
        unsigned short port_net;
    } serv_info;

    if (inet_pton(AF_INET, sinfo->ip, &serv_info.ip_net) <= 0) {
        perror("Error al convertir la dirección IP");
        break;
    }
    serv_info.ip_net = htonl(serv_info.ip_net); // Convertir IP a formato de red
    serv_info.port_net = htons(sinfo->port); // Convertir puerto a formato de red

    if (write(thinf->socket, &serv_info, sizeof(serv_info)) != sizeof(serv_info)) {
        perror("Error al enviar la información del servidor");
        break;
    }
    printf("Información del servidor enviada: IP %s, Puerto %d\n", sinfo->ip, sinfo->port);
    break;
}

            // Puedes agregar más casos aquí para otras operaciones (E: cErrar fichero, N: Nº ficheros, etc.)
        }
        
    }

    close(thinf->socket);
    free(thinf);
    printf("Conexión del cliente cerrada\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    int s, s_conec;
    unsigned int tam_dir;
    struct sockaddr_in dir_cliente;
    printf("master.c 2");

    if (argc!=2) {
        fprintf(stderr, "Uso: %s puerto\n", argv[0]);
        return -1;
    }
    server_list = array_create(1); // 1 para habilitar locking, 0 para deshabilitar
    if (!server_list) {
        perror("Error al crear la lista de servidores");
        return 1;
    }
    // inicializa el socket y lo prepara para aceptar conexiones
    if ((s=create_socket_srv(atoi(argv[1]), NULL)) < 0) return -1;
                    printf("master.c 3");
        init_master(); // Inicializar el maestro

    // prepara atributos adecuados para crear thread "detached"
    pthread_t thid;
    pthread_attr_t atrib_th;
    pthread_attr_init(&atrib_th); // evita pthread_join
    pthread_attr_setdetachstate(&atrib_th, PTHREAD_CREATE_DETACHED);
    printf("mastre.c 4");
    while(1) {
        tam_dir=sizeof(dir_cliente);
        // acepta la conexión
        if ((s_conec=accept(s, (struct sockaddr *)&dir_cliente, &tam_dir))<0){
            perror("error en accept");
            close(s);
            return -1;
        }
        printf("Conectado cliente con IP texto: %s y puerto %d (formato red)\n",
            inet_ntoa(dir_cliente.sin_addr), ntohs(dir_cliente.sin_port)); 
        printf("master.c 5");
        thread_info *thinf = malloc(sizeof(thread_info));
        thinf->socket=s_conec;
        pthread_create(&thid, &atrib_th, servicio, thinf);
    }
    close(s); // cierra el socket general
    return 0;
}
