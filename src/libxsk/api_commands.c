#include <net/if.h>

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "libxsk.h"
#include "socket99.h"
#include "lwlog.h"

#define PORT 8080

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

    lwlog_info("Sending to %s:%d", socket_cfg.host, socket_cfg.port);
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

    close(res.fd);
    return strdup(buffer);
}

void* tcp_server_nonblocking(void* arg) {
    bool* global_exit = arg;

    int v_true = 1;
    socket99_config cfg = {
        .host = "0.0.0.0",
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
        // Wait for incoming connections
        int nready = poll(fds, 1, 3000);
        if (nready < 0) {
            handle_error("poll failed");
            break;
        }
        if (nready == 0)
            continue;  // Timeout with no events

        if (fds[0].revents & POLLIN) {
            // Accept new connection
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
        // If no space is found, treat the entire data as a command with no argument
        handle_cmd(buf, NULL);
        free(buf);
        return;
    }

    // Replace the space with a null character to separate command and argument
    *space = '\0';
    char* arg = space + 1;
    if (*arg == '\0') {
        arg = NULL;
    }
    handle_cmd(buf, arg);
    free(buf);
}
static void load_config(char* veth_name, char* filename, bool peer) {
    char ifname[IFNAMSIZ];
    if (peer) {
        snprintf(ifname, IFNAMSIZ, "%s_peer", veth_name);
    } else {
        strncpy(ifname, veth_name, IFNAMSIZ);
    }

    struct config cfg = {
        .ifindex = -1,
        .unload_all = true,
        .ifname = ifname,
    };
    strncpy(cfg.filename, filename, 512);

    cfg.ifindex = if_nametoindex(ifname);
    if (cfg.ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", ifname);
        return;
    }

    if (load_xdp_program(&cfg, NULL, peer ? "xsks_map" : NULL) != 0) {
        lwlog_err("Couldn't load XDP program on iface '%s'", cfg.ifname);
    }
}

static void unload_config(char* veth_name, char* filename, bool peer) {
    char ifname[IFNAMSIZ];
    if (peer) {
        snprintf(ifname, IFNAMSIZ, "%s_peer", veth_name);
    } else {
        strncpy(ifname, veth_name, IFNAMSIZ);
    }

    struct config cfg = {
        .ifindex = -1,
        .unload_all = true,
        .ifname = ifname,
    };
    strncpy(cfg.filename, filename, 512);

    cfg.ifindex = if_nametoindex(ifname);
    if (cfg.ifindex == 0) {
        lwlog_err("Couldn't get ifindex for %s", ifname);
        return;
    }

    if (do_unload(&cfg) != 0) {
        lwlog_err("Couldn't unload XDP program on iface '%s'", cfg.ifname);
    }
}

void create_port(void* arg) {
    if (arg == NULL) {
        lwlog_err("create_port: arg is NULL");
        return;
    }

    char* veth_name = arg;
    lwlog_info("Creating veth pair: %s", veth_name);
    if (!create_veth(veth_name)) {
        lwlog_err("Couldn't create veth pair");
    }

    // Load peer af_xdp program
    load_config(veth_name, "obj/af_xdp.o", true);

    // Load non-peer dummy xdp program
    load_config(veth_name, "obj/xdp_dummy.o", false);

    int non_peer_ifindex = if_nametoindex(veth_name);
    int ret = update_devmap(non_peer_ifindex);
    if (ret == -1) {
        lwlog_err("Couldn't update devmap for %s", veth_name);
    }

    add_to_veth_list(veth_name);
}

void delete_port(void* arg) {
    if (arg == NULL) {
        lwlog_err("delete_port: arg is NULL");
        return;
    }

    char* veth_name = arg;

    // Unload peer af_xdp program
    unload_config(veth_name, "obj/af_xdp.o", true);

    // Unload non-peer dummy xdp program
    unload_config(veth_name, "obj/xdp_dummy.o", false);

    lwlog_info("Deleting veth pair: %s", veth_name);
    delete_veth(veth_name);

    int ret = remove_from_veth_list(veth_name);
    if (ret == -1) {
        lwlog_err("Couldn't remove %s from veth list", veth_name);
    }
}

int handle_cmd(char* cmd, void* arg) {
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

int update_devmap(int ifindex) {
    char pin_dir[PATH_MAX];
    struct bpf_map_info info = {0};

    // get ifname from ifindex
    char ifname[IF_NAMESIZE];
    if (if_indextoname(ifindex, ifname) == NULL) {
        lwlog_err("Couldn't get ifname from ifindex %d", ifindex);
        return -1;
    }

    int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, phy_ifname);
    if (len < 0 || len >= PATH_MAX) {
        lwlog_err("Couldn't format pin_dir");
        return -1;
    }

    int map_fd = open_bpf_map_file(pin_dir, "xdp_devmap", &info);
    if (map_fd < 0) {
        lwlog_err("Couldn't open xdp_devmap");
        return -1;
    }

    int key = 0;
    int ret = bpf_map_update_elem(map_fd, &key, &ifindex, BPF_ANY);

    if (ret) {
        lwlog_info("Couldn't update devmap for %s", ifname);
        return -1;
    }

    lwlog_info("Redirecting packets from %s to %s", phy_ifname, ifname);
    return 0;
}