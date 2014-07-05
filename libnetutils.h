/* utilities functions */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define MAX_BUFFER 128

/*****************************************************************************/
/* General */
/*****************************************************************************/

int create_socket (int family) {
    int socket_id;

    /* run socket function */
    socket_id = socket(AF_INET, family, 0);

    /* if error occur - send message to user */
    if (socket_id == -1) {
        fprintf(stderr, "ERROR: unable to create socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
        return socket_id;
    }

    printf("[+] Create socket %d\n", socket_id);

    return socket_id;
}

int create_socket_udp () {
    return create_socket(SOCK_DGRAM);
}

int create_socket_tcp () {
    return create_socket(SOCK_STREAM);
}

int close_connection (int socket) {
     int status = close(socket);

     if (status == -1) {
        fprintf(stderr, "ERROR: unable to close connection for socket %d: %s\n", socket, strerror(errno));
        exit(EXIT_FAILURE);
        return status;
     }

     printf("[+] Close connection with socket %d\n", socket);

     return status;
}

int bind_port (int socket, int port) {
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    memset( &( address.sin_zero ), '\0', 8 );

    int bind_status = bind(socket, (struct sockaddr *) & address, sizeof(struct sockaddr));
    // unsigned short port = ntohs(address.sin_port);

    if (bind_status == -1) {
        fprintf(stderr, "ERROR: unable to bind port %d: %s\n", port, strerror(errno));
        exit(EXIT_FAILURE);
        return bind_status;
    }

    printf("[+] Bind port %d\n", port);

    return bind_status;
}

void demonize () {
    pid_t pid, sid;
    int chdir_status, fd;

    /* check if already is daemon */
    if ( getppid() == 1 ) {
        fprintf(stderr, "ERROR: unable to demonize, becouse is already daemon\n");
        exit(EXIT_FAILURE);
    }

    /* fork current process */
    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "ERROR: unable to demonize: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* parent dead */
        exit(EXIT_SUCCESS);
    }

    /* create a new SID for the child process */
    sid = setsid();

    signal(SIGHUP,SIG_IGN);

    pid = fork();

    if (pid != 0) {
        /* first child dead */
        exit(EXIT_SUCCESS);
    }

    if (sid < 0) {
        fprintf(stderr, "ERROR: unable to create new SID: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* change the current working directory */
    chdir_status = chdir("/");

    if (chdir_status < 0) {
        fprintf(stderr, "ERROR: unable to change directory to /: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* close other descriptors */
    fd = open("/dev/null", O_RDWR, 0);

    if (fd != -1) {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);

        if (fd > 2) {
            close (fd);
        }
    }

    /* resetting file creation mask */
    umask(027);
}

void handle_incoming_client (int socket, void (*func)(), int use_syslog) {
    int fresh_socket, close_status;
    struct sockaddr_in address;
    socklen_t address_len = sizeof(struct sockaddr_in);
    struct in_addr client_ip;
    char * ip;
    unsigned short port;

    /* waiting for something happen */
    while ( 1 ) {
        /* accept connection with client from from queue */
        fresh_socket = accept(socket, (struct sockaddr *) & address, & address_len);
        client_ip = address.sin_addr;
        ip = inet_ntoa(client_ip);
        port = ntohs(address.sin_port);

        if (fresh_socket == -1) {
            fprintf(stderr, "ERROR: unable to accept client %s:%d: %s\n", ip, port, strerror(errno));
            exit(EXIT_FAILURE);
        }

        printf("[+] Accept client %s:%d\n", ip, port);

        /* send message to client */
        func(fresh_socket);
        // send_time_to_socket(fresh_socket);

        /* if we are would like use syslog */
        if (use_syslog == 1) {
            syslog(LOG_NOTICE, "Successfully serve client %s:%d", ip, port);
        }

        /* close connection with client */
        close_status = close(fresh_socket);

        if (close_status == -1) {
            fprintf(stderr, "ERROR: unable to close connection with client %s:%d: %s\n", ip, port, strerror(errno));
            exit(EXIT_FAILURE);
        }

        printf("[+] Close client %s:%d\n\n", ip, port);
    }
}

void receive_from_server (int socket) {
    int in;
    char buffer[MAX_BUFFER + 1];

    /* get data from client */
    while ((in = read(socket, buffer, MAX_BUFFER)) > 0) {
        buffer[in] = 0;
        printf("\n%s", buffer);
    }
}

/*****************************************************************************/
/* TCP */
/*****************************************************************************/

int create_connection_tcp (int socket, const char * ip, int port) {
    int connect_status;
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);
    memset( &( address.sin_zero ), '\0', 8 );

    printf("[-] Trying connection %s:%d...\n", ip, port);

    connect_status = connect(socket, (struct sockaddr *) & address, sizeof(struct sockaddr));

    if (connect_status == -1) {
        fprintf(stderr, "ERROR: unable to create connection to socket %d: %s\n", socket, strerror(errno));
        exit(EXIT_FAILURE);
        return connect_status;
    }

    printf("[+] Connected with socket %d\n", socket);

    return connect_status;
}

int listen_for_client_tcp (int socket, int backlog) {
    int listen_status = listen(socket, backlog);

    if (listen_status == -1) {
        fprintf(stderr, "ERROR: unable to bind for socket %d with backlog %d: %s\n", socket, backlog, strerror(errno));
        exit(EXIT_FAILURE);
        return listen_status;
    }

    printf("[+] Listening for socket %d with backlog %d\n", socket, backlog);

    return listen_status;
}

/*****************************************************************************/
/* Custom */
/*****************************************************************************/

void send_time_to_socket (int socket) {
    char buffer[MAX_BUFFER + 1];
    time_t current_time;

    current_time = time(NULL);
    snprintf(buffer, MAX_BUFFER, "%s\n", ctime(&current_time));

    write(socket, buffer, strlen(buffer));
}

void send_uptime_to_socket (int socket) {
    char buffer[MAX_BUFFER + 1];
    snprintf(buffer, MAX_BUFFER, "%d\n", shellcmd("uptime"));
    write(socket, buffer, strlen(buffer));
}

void send_empty_datagram (int socket) {
    /* TODO(piecioshka): fill it */
}
