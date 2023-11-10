#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include "daemon_api.h"
#include "lwlog.h"
#include "socket99.h"
#include "defs.h"
#include "xdp_loader.h"
#include "xdp_daemon_utils.h"

#define PORT 8080

typedef void (*CommandHandler)(void*);

typedef struct {
    char* command;
    CommandHandler handler;
} Command;

Command commands[] = {
    {"create_port", create_port},
    {"delete_port", delete_port},
};

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

    if (!(fd.revents & POLLOUT)) {
        if (fd.revents & (POLLERR | POLLHUP)) {
            lwlog_err("poll: POLLERR or POLLHUP");
        }
        close(res.fd);
        return NULL;
    }

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
    close(res.fd);
    return strdup(buffer);
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

            handle_client(client_fd, global_exit);
            close(client_fd);
        }
    }

    lwlog_info("Exiting TCP server");
    close(res.fd);
    return NULL;
}

void handle_client(int client_fd, bool* global_exit) {
    if (global_exit == NULL || *global_exit) {
        lwlog_info("Exiting TCP server");
        return;
    }

    char* buf = malloc(1024);
    if (buf == NULL) {
        handle_error("malloc failed");
        return;
    }

    ssize_t received = recv(client_fd, buf, 1024 - 1, 0);
    if (received <= 0) {
        if (received == 0) {
            lwlog_info("Client disconnected");
        } else {
            handle_error("recv failed");
        }
        free(buf);
        return;
    }

    buf[received] = '\0';
    char* space = strchr(buf, ' ');
    if (space == NULL) {
        handlecmd(buf, NULL);
        free(buf);
        return;
    }

    *space = '\0';
    char* arg = space + 1;
    if (*arg == '\0') {
        arg = NULL;
    }
    handlecmd(buf, arg);
    free(buf);
}

void create_port(void* arg) {
    char* veth_name = arg;
    lwlog_info("Creating veth pair: %s", veth_name);
    if (!create_veth(veth_name)) {
        lwlog_err("Couldn't create veth pair");
    }

    // Load XDP program
    struct config cfg = {
        .ifindex = -1,
        .unload_all = true,
        .filename = "obj/xdp_kern_obj.o",
        .ifname = veth_name,
    };

    cfg.ifindex = if_nametoindex(veth_name);
    if (cfg.ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", veth_name);
        return;
    }

    lwlog_info("Loading XDP program");
    if (load_xdp_program(&cfg, NULL) != 0) {
        lwlog_err("Couldn't load XDP program");
    }

    add_to_veth_list(veth_name);
}

void delete_port(void* arg) {
    char* veth_name = arg;
    struct config cfg = {
        .ifindex = -1,
        .unload_all = true,
        .filename = "obj/xdp_kern_obj.o",
        .ifname = veth_name,
    };

    cfg.ifindex = if_nametoindex(veth_name);
    if (cfg.ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", veth_name);
        return;
    }

    lwlog_info("Unloading XDP program");
    if (do_unload(&cfg) != 0) {
        lwlog_err("Couldn't unload XDP program");
    }

    lwlog_info("Deleting veth pair: %s", veth_name);
    if (!delete_veth(veth_name)) {
        lwlog_err("Couldn't delete veth pair");
    }

    int ret = remove_from_veth_list(veth_name);
    if (ret == -1) {
        lwlog_err("Couldn't remove %s from veth list", veth_name);
    }
}

int handlecmd(char* cmd, void* arg) {
    lwlog_info("Received command: %s", cmd);
    for (int i = 0; i < sizeof(commands) / sizeof(Command); i++) {
        if (strcmp(cmd, commands[i].command) == 0) {
            commands[i].handler(arg);
            return 0;
        }
    }

    lwlog_err("Unknown command: %s", cmd);
    return -1;
}

static inline void handle_error(const char* msg) {
    lwlog_err("%s: %s", msg, strerror(errno));
    errno = 0;
}