// IMPLEMENTACIÓN DE LA BIBLIOTECA DE CLIENTE.
// PUEDE USAR EL EJEMPLO DE SOCKETS cliente.c COMO PUNTO DE PARTIDA.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "common_cln.h"
#include "master.h"
#include "server.h"
#include "mgfs.h"
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// TIPOS INTERNOS

// descriptor de un sistema de ficheros:
// tipo interno que almacena información de un sistema de ficheros
typedef struct mgfs_fs {
    int sockfd;
    int def_blocksize;
    int def_rep_factor;
} mgfs_fs;

// descriptor de un fichero:
// tipo interno que almacena información de un fichero
typedef struct mgfs_file {
    mgfs_fs *fs;
    char *fname;
    int blocksize;
    int rep_factor;
} mgfs_file;

/* 
 * FASE 1: CREACIÓN DE FICHEROS
 */

// PASO 1: CONEXIÓN Y DESCONEXIÓN

// Establece una conexión con el sistema de ficheros especificado,
// fijando los valores por defecto para el tamaño de bloque y el
// factor de replicación para los ficheros creados en esta sesión.
// Devuelve el descriptor del s. ficheros si OK y NULL en caso de error.
mgfs_fs * mgfs_connect(const char *master_host, const char *master_port,
                       int def_blocksize, int def_rep_factor) {
    int sockfd = create_socket_cln_by_name(master_host, master_port);
    if (sockfd < 0) {
        return NULL;
    }
    printf("Connecting");
    printf("Se conecta a %s, y a %s", master_host, master_port);
    mgfs_fs *fs = (mgfs_fs *)malloc(sizeof(mgfs_fs));
    if (fs == NULL) {
        close(sockfd);
        return NULL;
    }
    printf("Connecting2");
    fs->sockfd = sockfd;
    fs->def_blocksize = def_blocksize;
    fs->def_rep_factor = def_rep_factor;
    printf("Connecting3 ");
    return fs;
}

int mgfs_disconnect(mgfs_fs *fs) {
    if (fs == NULL) {
        return -1;
    }

    close(fs->sockfd);
    free(fs);

    return 0;
}

int mgfs_get_def_blocksize(const mgfs_fs *fs) {
    if (fs == NULL) {
        return -1;
    }

    return fs->def_blocksize;
}

int mgfs_get_def_rep_factor(const mgfs_fs *fs) {
    if (fs == NULL) {
        return -1;
    }

    return fs->def_rep_factor;
}

// PASO 2: CREAR FICHERO

// Crea un fichero con los parámetros especificados.
// Si blocksize es 0, usa el valor por defecto.
// Si rep_factor es 0, usa el valor por defecto.
// Devuelve el descriptor del fichero si OK y NULL en caso de error.
mgfs_file *mgfs_create(const mgfs_fs *fs, const char *fname, int blocksize, int rep_factor) {
    if (fs == NULL || fname == NULL) {
        return NULL;
    }
    perror("create 1");

    mgfs_file *file = (mgfs_file *)malloc(sizeof(mgfs_file));
    if (file == NULL) {
        return NULL;
    }
    perror("create 2");

    file->blocksize = blocksize > 0 ? blocksize : fs->def_blocksize;
    file->rep_factor = rep_factor > 0 ? rep_factor : fs->def_rep_factor;
    file->fname = strdup(fname);
    if (file->fname == NULL) {
        free(file);
        return NULL;
    }
    file->fs = (mgfs_fs *)fs;

    char op_code = 'C'; // Código de operación para crear un fichero
    int fname_len = strlen(fname) + 1; // Incluir el carácter nulo
    int blocksize_net = htonl(file->blocksize);
    int rep_factor_net = htonl(file->rep_factor);
    int fname_len_net = htonl(fname_len); // Asegúrate de convertir a formato de red

    perror("create 3");

    struct iovec iov[5];
    iov[0].iov_base = &op_code;
    iov[0].iov_len = sizeof(op_code);
    iov[1].iov_base = &blocksize_net;
    iov[1].iov_len = sizeof(blocksize_net);
    iov[2].iov_base = &rep_factor_net;
    iov[2].iov_len = sizeof(rep_factor_net);
    iov[3].iov_base = &fname_len_net; // Enviar la longitud del nombre del fichero en formato de red
    iov[3].iov_len = sizeof(fname_len_net);
    iov[4].iov_base = file->fname;
    iov[4].iov_len = fname_len;

    if (writev(fs->sockfd, iov, 5) < 0) {
        perror("Error al enviar datos al maestro");
        free(file->fname);
        free(file);
        return NULL;
    }
    perror("create 4");

    int success;
    if (read(fs->sockfd, &success, sizeof(success)) <= 0) {
        perror("Error al recibir respuesta del maestro");
        free(file->fname);
        free(file);
        return NULL;
    }
    perror("create 5");

    success = ntohl(success); // Convertir del formato de red al formato de host

    if (success < 0) {
        free(file->fname);
        free(file);
        return NULL;
    }

    return file;
}


// Cierra un fichero.
// Devuelve 0 si OK y un valor negativo si error.
int mgfs_close(mgfs_file *f) {
    if (f == NULL) {
        return -1;
    }
    free(f->fname);
    free(f);
    return 0;
}
// Devuelve tamaño de bloque y un valor negativo en caso de error.
int mgfs_get_blocksize(const mgfs_file *f) {
    if (f == NULL) {
        return -1;
    }
    return f->blocksize;
}

// Devuelve factor de replicación y valor negativo en caso de error.
int mgfs_get_rep_factor(const mgfs_file *f) {
    if (f == NULL) {
        return -1;
    }
    return f->rep_factor;
}

// Devuelve el nº ficheros existentes y un valor negativo si error.
int _mgfs_nfiles(const mgfs_fs *fs) {
    char op_code = 'N'; // Código de operación para número de ficheros
    perror("Antes de enviar código de operación 'N'");
    if (write(fs->sockfd, &op_code, sizeof(op_code)) < 0) {
            perror("Error al enviar código de operación 'N'");
        return -1;
    }
    int n_files;
    if (read(fs->sockfd, &n_files, sizeof(n_files)) <= 0) {
        return -1;
    }
        n_files = ntohl(n_files); // Convertir del formato de red al formato de host
    return n_files;
}


// PASO 3: APERTURA DE FICHERO PARA LECTURA

// Abre un fichero para su lectura.
// Devuelve el descriptor del fichero si OK y NULL en caso de error.
mgfs_file *mgfs_open(const mgfs_fs *fs, const char *fname) {
    if (fs == NULL || fname == NULL) {
        return NULL;
    }
        perror("open 1");

    mgfs_file *file = (mgfs_file *)malloc(sizeof(mgfs_file));
    if (file == NULL) {
        return NULL;
    }

    file->fname = strdup(fname);
    if (file->fname == NULL) {
        free(file);
        return NULL;
    }

    char op_code = 'O'; // Código de operación para abrir un fichero
    int fname_len = strlen(fname) + 1; // Incluir el carácter nulo
    int fname_len_net = htonl(fname_len); // Convertir a formato de red
        perror("open 2");

    struct iovec iov[3];
    iov[0].iov_base = &op_code;
    iov[0].iov_len = sizeof(op_code);
    iov[1].iov_base = &fname_len_net; // Enviar la longitud del nombre del fichero en formato de red
    iov[1].iov_len = sizeof(fname_len_net);
    iov[2].iov_base = file->fname;
    iov[2].iov_len = fname_len;
        perror("open 3");

    if (writev(fs->sockfd, iov, 3) < 0) {
        perror("Error al enviar datos al maestro");
        free(file->fname);
        free(file);
        return NULL;
    }
            perror("open 4");

    // Recibir el tamaño del bloque y el factor de replicación
     // Recibir el tamaño del bloque y el factor de replicación o un error
    int blocksize_net, rep_factor_net;
    if (read(fs->sockfd, &blocksize_net, sizeof(blocksize_net)) <= 0) {
        perror("Error al recibir tamaño del bloque del maestro");
        free(file->fname);
        free(file);
        return NULL;
    }

    // Verificar si recibimos un error
    if (ntohl(blocksize_net) == -1) {
        free(file->fname);
        free(file);
        return NULL;
    }

    if (read(fs->sockfd, &rep_factor_net, sizeof(rep_factor_net)) <= 0) {
        perror("Error al recibir factor de replicación del maestro");
        free(file->fname);
        free(file);
        return NULL;
    }
            perror("open 5");

    file->blocksize = ntohl(blocksize_net); // Convertir del formato de red al formato de host
    file->rep_factor = ntohl(rep_factor_net); // Convertir del formato de red al formato de host
    file->fs = (mgfs_fs *)fs;

    return file;
}

/* 
 * FASE 2: ALTA DE LOS SERVIDORES
 */

// Operación interna para test; no para uso de las aplicaciones.
// Obtiene la información de localización (ip y puerto en formato de red)
// de un servidor.
// Devuelve 0 si OK y un valor negativo si error.
int _mgfs_serv_info(const mgfs_fs *fs, int n_server, unsigned int *ip, unsigned short *port) {
    if (fs == NULL || ip == NULL || port == NULL) {
        fprintf(stderr, "Invalid arguments: fs, ip, or port is NULL\n");
        return -1;
    }

    char op_code = 'S'; // Código de operación para información del servidor
    int n_server_net = htonl(n_server);

    struct iovec iov[2];
    iov[0].iov_base = &op_code;
    iov[0].iov_len = sizeof(op_code);
    iov[1].iov_base = &n_server_net;
    iov[1].iov_len = sizeof(n_server_net);

    printf("Sending op_code: %c, n_server_net: %d\n", op_code, n_server_net);

    if (writev(fs->sockfd, iov, 2) < 0) {
        perror("Error al enviar datos al maestro");
        return -1;
    }

    struct {
        unsigned int ip_net;
        unsigned short port_net;
    } serv_info;

    printf("Reading server info...\n");

    ssize_t read_bytes = read(fs->sockfd, &serv_info, sizeof(serv_info));
    if (read_bytes != sizeof(serv_info)) {
        perror("Error al recibir datos del maestro");
        fprintf(stderr, "Expected %lu bytes, but read %zd bytes\n", sizeof(serv_info), read_bytes);
        return -1;
    }

    *ip = ntohl(serv_info.ip_net);
    *port = ntohs(serv_info.port_net);

    printf("Received IP: %u, Port: %u\n", *ip, *port);

    return 0;
}


/* 
 * FASE 3: ASIGNACIÓN DE SERVIDORES A BLOQUES.
 */

// Operación interna: será usada por write.
// Asigna servidores a las réplicas del siguiente bloque del fichero.
// Devuelve la información de localización (ip y puerto en formato de red)
// de cada una de ellas.
// Retorna 0 si OK y un valor negativo si error.
int _mgfs_alloc_next_block(const mgfs_file *file, unsigned int *ips, unsigned short *ports) {
    return 0;
}

// Obtiene la información de localización (ip y puerto en formato de red)
// de los servidores asignados a las réplicas del bloque.
// Retorna 0 si OK y un valor negativo si error.
int _mgfs_get_block_allocation(const mgfs_file *file, int n_bloque,
        unsigned int *ips, unsigned short *ports) {
    return 0;
}

/*
 * FASE 4: ESCRITURA EN EL FICHERO.
 */

// Escritura en el fichero.
// Devuelve el tamaño escrito si OK y un valor negativo si error.
// Por restricciones de la práctica, "size" tiene que ser múltiplo
// del tamaño de bloque y el valor devuelto deber ser igual a "size".
int mgfs_write(mgfs_file *file, const void *buff, unsigned long size){
    if (size % mgfs_get_blocksize(file)) return -1;
    return 0;
}

