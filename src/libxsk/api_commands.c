#include <net/if.h>
#include <linux/if.h>

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

typedef void (*CommandHandler)(const void*);

typedef struct {
    char* command;
    CommandHandler handler;
} Command;

Command commands[] = {
    {"create_port", create_port},
    {"delete_port", delete_port},
};

char* send_to_daemon(const char* msg) {
    if (msg == NULL) {
        lwlog_err("msg is NULL");
        return NULL;
    }

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

    const ssize_t sent = send(res.fd, msg, strlen(msg), 0);
    if (sent == -1) {
        lwlog_err("send: %s", strerror(errno));
        close(res.fd);
        return NULL;
    }

    fd.events = POLLIN;
    char buffer[1024] = {0};
    const ssize_t valread = recv(res.fd, buffer, sizeof(buffer) - 1, 0);
    if (valread == -1) {
        lwlog_err("recv: %s", strerror(errno));
        close(res.fd);
        return NULL;
    }

    close(res.fd);
    return strdup(buffer);
}

void* tcp_server_nonblocking(void* arg) {
    if (arg == NULL) {
        lwlog_err("arg is NULL");
        return NULL;
    }

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
        const int nready = poll(fds, 1, 3000);
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
            const int client_fd = accept(res.fd, (struct sockaddr*)&client_addr, &client_len);
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

void handle_client(int client_fd, const bool* global_exit) {
    if (global_exit == NULL || *global_exit) {
        lwlog_info("Exiting TCP server");
        return;
    }

    if (client_fd < 0) {
        handle_error("client_fd is negative");
        return;
    }

    char* buf = malloc(1024);
    if (buf == NULL) {
        handle_error("malloc failed");
        return;
    }

    const ssize_t received = recv(client_fd, buf, 1024 - 1, 0);
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

static void load_pair_config(const struct veth_pair* pair) {
    if (!pair) {
        lwlog_err("pair is NULL");
        return;
    }
    lwlog_info("Loading XDP program on %s[%d] and %s[%d]", pair->veth_outer, pair->outer_ifindex, pair->veth_inner,
               pair->inner_ifindex);

    struct config cfg = {
        .ifindex = pair->outer_ifindex,
        .ifname = pair->veth_outer,
        .filename = dummy_prog_path,
    };

    if (load_xdp_program(&cfg, NULL) != 0) {
        lwlog_err("Couldn't load XDP program on iface '%s'", cfg.ifname);
    }

    cfg.ifindex = pair->inner_ifindex;
    cfg.ifname = pair->veth_inner;
    cfg.filename = af_xdp_prog_path;
    if (load_xdp_program(&cfg, "xsks_map") != 0) {
        lwlog_err("Couldn't load XDP program on iface '%s'", cfg.ifname);
    }
}

static void unload_pair_config(const struct veth_pair* pair) {
    if (!pair) {
        lwlog_err("pair is NULL");
        return;
    }

    struct config cfg;
    init_empty_config(&cfg);
    cfg.ifindex = pair->outer_ifindex;
    cfg.ifname = pair->veth_outer;
    cfg.filename = dummy_prog_path;

    if (do_unload(&cfg) != 0) {
        lwlog_err("Couldn't unload XDP program on iface '%s'", cfg.ifname);
    }

    cfg.ifindex = pair->inner_ifindex;
    cfg.ifname = pair->veth_inner;
    cfg.filename = af_xdp_prog_path;
    if (do_unload(&cfg) != 0) {
        lwlog_err("Couldn't unload XDP program on iface '%s'", cfg.ifname);
    }
}

int handle_cmd(char* cmd, void* arg) {
    if (cmd == NULL) {
        lwlog_err("cmd is NULL");
        return -1;
    }

    if (arg == NULL) {
        lwlog_err("arg is NULL");
        return -1;
    }

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

void create_port(const void* arg) {
    if (!arg) {
        lwlog_err("arg is NULL");
        return;
    }

    const char* prefix = arg;
    const int index = add_to_veth_list(prefix);
    if (index == -1) {
        lwlog_err("Couldn't add %s to veth list", prefix);
        return;
    }

    lwlog_info("Creating veth pair: [%s, %s]", get_index(index)->veth_outer, get_index(index)->veth_inner);
    create_veth(get_index(index)->veth_outer, get_index(index)->veth_inner);
    get_index(index)->outer_ifindex = if_nametoindex(get_index(index)->veth_outer);
    get_index(index)->inner_ifindex = if_nametoindex(get_index(index)->veth_inner);
    load_pair_config(get_index(index));

    const int ret = update_devmap(get_index(index)->outer_ifindex, get_index(index)->veth_outer);
    if (ret) {
        lwlog_err("Couldn't update devmap for %s", get_index(index)->veth_outer);
    }
}

void delete_port(const void* arg) {
    if (!arg) {
        lwlog_err("arg is NULL");
        return;
    }

    const char* prefix = arg;
    const struct veth_pair* pair = get_pair(prefix);
    if (pair == NULL) {
        lwlog_err("Couldn't find veth pair with prefix %s", prefix);
        return;
    }
    lwlog_info("Deleting veth pair: [%s, %s]", pair->veth_outer, pair->veth_inner);
    unload_pair_config(pair);
    delete_veth(pair->veth_outer);
    remove_from_veth_list(pair->veth_outer);
}

int update_devmap(int ifindex, char* ifname) {
    char pin_dir[PATH_MAX] = {0};
    struct bpf_map_info info = {0};

    const int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, phy_ifname);
    if (len < 0 || len >= PATH_MAX) {
        lwlog_err("Couldn't format pin_dir");
        return -1;
    }

    const int map_fd = open_bpf_map_file(pin_dir, "xdp_devmap", &info);
    if (map_fd < 0) {
        lwlog_err("Couldn't open xdp_devmap");
        return -1;
    }

    const int key = 0;
    const int ret = bpf_map_update_elem(map_fd, &key, &ifindex, BPF_ANY);
    if (ret) {
        lwlog_info("Couldn't update devmap for %s", ifname);
        return -1;
    }
    return 0;
}
