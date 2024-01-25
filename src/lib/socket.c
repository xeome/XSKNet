#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include "socket.h"
#include "lwlog.h"
#include "socket_handler.h"
#include "veth_list.h"

pthread_t socket_thread = 0;
int client_socket_fd;
int server_socket_fd;
enum { TIMEOUT_SECONDS = 5, MAX_CLIENTS = 10 };

/*
 * Creates a socket
 */
int socket_create() {
    lwlog_info("Creating socket");
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return socket_fd;
}

/*
 * Binds a socket to a port
 */
void socket_bind(int socket_fd, const int port) {
    lwlog_info("Binding socket to port %d", port);
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
}

/*
 * Starts listening on a socket
 */
void socket_listen(const int socket_fd) {
    lwlog_info("Listening on socket");
    if (listen(socket_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

/*
 * Accepts a connection on a socket
 */
int socket_accept(const int socket_fd) {
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);
    const int fd = accept(socket_fd, (struct sockaddr*)&client_address, &client_address_len);
    if (fd < 0) {
        return -1;
    }
    return fd;
}

/*
 * Connects a socket to a server
 */
void socket_connect(const int socket_fd, const char* server_address, const int port) {
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, server_address, &server.sin_addr) < 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if (connect(socket_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
}

/*
 * Reads data from a socket
 */
long socket_read(const int socket_fd, void* buffer, const int buffer_size) {
    const long bytes_read = read(socket_fd, buffer, buffer_size);
    if (bytes_read < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    return bytes_read;
}

/*
 * Writes data to a socket
 */
void socket_write(const int socket_fd, const void* buffer, const int buffer_size) {
    if (write(socket_fd, buffer, buffer_size) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }
}

/*
 * Closes a socket
 */
void socket_close(const int socket_fd) {
    if (close(socket_fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

/*
 * Wrapper for socket_create, socket_bind, and socket_listen, used by the server, passed to the server's pthread_create will run
 * programs whole lifetime
 */
void* socket_server_thread_func(void* exit_flag) {
    lwlog_info("Starting socket server thread");
    const int socket_fd = socket_create();
    socket_bind(socket_fd, 8080);
    socket_listen(socket_fd);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        lwlog_err("setsockopt(SO_REUSEADDR) failed");
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0)
        lwlog_err("setsockopt(SO_REUSEPORT) failed");

    while (*(int*)exit_flag == 0) {
        const int fd = socket_accept(socket_fd);
        if (fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            } else {
                lwlog_crit("Accept failed");
                break;
            }
        }
        handle_client(fd);
    }
    socket_close(socket_fd);
    return NULL;
}

void socket_write_with_timeout(const int socket_fd, void* buffer, const unsigned long buffer_size) {
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(socket_fd, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    int result = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);

    if (result == -1) {
        perror("select");
        exit(EXIT_FAILURE);
    } else if (result == 0) {
        lwlog_err("Timeout while waiting for server connection");
        exit(EXIT_FAILURE);
    }

    if (write(socket_fd, buffer, buffer_size) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    char response[1024];
    const long bytes_read = read(socket_fd, response, sizeof(response));
    if (bytes_read < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    lwlog_info("Server response: %s", response);

    // Copy response string to buffer
    strncpy(buffer, response, buffer_size);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
}

/*
 * Function for sending data to the server, used by the client, separate process so it will use port as parameter
 */
void socket_send_to_port(char* cmd, int port) {
    const int socket_fd = socket_create();
    lwlog_info("Connecting to server on port %d", port);
    socket_connect(socket_fd, "127.0.0.1", port);

    lwlog_info("Sending command to server: %s", cmd);
    socket_write_with_timeout(socket_fd, cmd, strlen(cmd));

    lwlog_info("Closing socket");
    socket_close(socket_fd);
}

void request_port(const char* veth_name) {
    lwlog_info("Requesting port %s", veth_name);
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "create_port %s", veth_name);
    socket_send_to_port(buffer, 8080);
}

void remove_port(const char* veth_name) {
    lwlog_info("Deleting port %s", veth_name);
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "delete_port %s", veth_name);
    socket_send_to_port(buffer, 8080);
}

void request_phy_ifname(char* veth_name) {
    lwlog_info("Requesting phy_ifname for %s", veth_name);
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "get_phy_if %s", veth_name);
    socket_send_to_port(buffer, 8080);
    lwlog_info("phy_ifname: %s", buffer);
    strncpy(veth_name, buffer, IFNAMSIZ);  // NOLINT(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
}