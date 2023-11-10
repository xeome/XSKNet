#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include "daemon_api.h"
#include "lwlog.h"
#include "socket99.h"

#define PORT 8080

char* send_to_daemon(char* msg) {
    socket99_config socket_cfg = {
        .host = "127.0.0.1",
        .port = 8080,
    };

    socket99_result res;
    if (!socket99_open(&socket_cfg, &res)) {
        socket99_fprintf(stderr, &res);
        return NULL;
    }

    const int TIMEOUT_MSEC = 10 * 1000;

    struct pollfd fd = {.fd = res.fd, .events = POLLOUT};
    if (poll(&fd, 1, TIMEOUT_MSEC) <= 0) {
        lwlog_err("poll: %s", strerror(errno));
        close(res.fd);
        errno = 0;
        return NULL;
    }

    if (fd.revents & POLLOUT) {
        ssize_t sent = send(res.fd, msg, strlen(msg), 0);
        lwlog_info("sent: %ld bytes", sent);
        if (sent == -1) {
            lwlog_err("send: %s", strerror(errno));
            close(res.fd);
            return NULL;
        }

        fd.events = POLLIN;
        char buffer[1024] = {0};
        ssize_t valread = recv(res.fd, buffer, sizeof(buffer) - 1, 0);
        if (valread == -1) {
            lwlog_err("recv: %s", strerror(errno));
            close(res.fd);
            return NULL;
        }

        lwlog_info("received: %s", buffer);
        return strdup(buffer);
    } else if (fd.revents & (POLLERR | POLLHUP)) {
        lwlog_err("poll: POLLERR or POLLHUP");
        close(res.fd);
        return NULL;
    }

    close(res.fd);
    return NULL;
}

void* tcp_server_nonblocking(void* arg) {
    bool* global_exit = arg;

    int v_true = 1;
    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = PORT,
        .server = true,
        .nonblocking = true,
        .sockopts =
            {
                {SO_REUSEADDR, &v_true, sizeof(v_true)},
            },

    };

    socket99_result res;
    if (!socket99_open(&cfg, &res)) {
        handle_error("socket99_open failed");
        return NULL;
    }

    struct pollfd fds[2] = {{.fd = res.fd, .events = POLLIN}};

    while (!*global_exit) {
        int nready = poll(fds, 1, 3000);
        if (nready < 0) {
            handle_error("poll failed");
            break;
        }
        if (nready == 0)
            continue;  // Timeout with no events

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(res.fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (errno != EAGAIN) {
                    handle_error("accept failed");
                    break;
                }
                continue;
            }

            handle_client(client_fd);
            close(client_fd);
        }
    }

    lwlog_info("Exiting TCP server");
    close(res.fd);
    return NULL;
}

void handle_client(int client_fd) {
    char buf[1024];
    ssize_t received = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (received > 0) {
        buf[received] = '\0';
        lwlog_info("Received: %s", buf);
    } else if (received == 0) {
        lwlog_info("Client disconnected");
    } else {
        handle_error("recv failed");
    }
}

static inline void handle_error(const char* msg) {
    lwlog_err("%s: %s", msg, strerror(errno));
    errno = 0;
}