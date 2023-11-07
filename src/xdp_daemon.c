#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <sys/resource.h>
#include <pthread.h>

#include <bpf/bpf.h>
#include <xdp/xsk.h>

#include "xdp_daemon.h"
#include "xdp_load.h"
#include "xdp_socket.h"
#include "xdp_receive.h"
#include "lwlog.h"
#include "socket_stats.h"
#include "socket99.h"

#define PORT 8080
int xsk_map_fd;
bool global_exit;
struct xdp_program* prog;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

static const char* __doc__ = "AF_XDP kernel bypass example\n";

const struct option_wrapper long_options[] = {

    {{"help", no_argument, NULL, 'h'}, "Show help", false},

    {{"dev", required_argument, NULL, 'd'}, "Operate on device <ifname>", "<ifname>", true},

    {{"skb-mode", no_argument, NULL, 'S'}, "Install XDP program in SKB (AKA generic) mode"},

    {{"native-mode", no_argument, NULL, 'N'}, "Install XDP program in native mode"},

    {{"auto-mode", no_argument, NULL, 'A'}, "Auto-detect SKB or native mode"},

    {{"force", no_argument, NULL, 'F'}, "Force install, replacing existing program on interface"},

    {{"copy", no_argument, NULL, 'c'}, "Force copy mode"},

    {{"zero-copy", no_argument, NULL, 'z'}, "Force zero-copy mode"},

    {{"queue", required_argument, NULL, 'Q'}, "Configure interface receive queue for AF_XDP, default=0"},

    {{"poll-mode", no_argument, NULL, 'p'}, "Use the poll() API waiting for packets to arrive"},

    {{"quiet", no_argument, NULL, 'q'}, "Quiet mode (no output)"},

    {{"filename", required_argument, NULL, 1}, "Load program from <file>", "<file>"},

    {{"progname", required_argument, NULL, 2}, "Load program from function <name> in the ELF file", "<name>"},

    {{0, 0, NULL, 0}, NULL, false}};

int main(int argc, char** argv) {
    int err;
    global_exit = false;

    signal(SIGINT, exit_application);
    signal(SIGTERM, exit_application);

    lwlog_info("Starting XDP Daemon");

    /* Cmdline options can change progname */
    parse_cmdline_args(argc, argv, long_options, &cfg, __doc__);

    /* Required option */
    if (cfg.ifindex == -1) {
        lwlog_crit("ERROR: Required option --dev missing\n\n");
        usage(argv[0], __doc__, long_options, (argc == 1));
        return EXIT_FAIL_OPTION;
    }

    /* Load XDP kernel program */
    err = load_xdp_program(&cfg, prog, &xsk_map_fd);
    if (err) {
        lwlog_crit("ERROR: loading program: %s\n", strerror(err));
        exit(1);
    }

    // /* Allow unlimited locking of memory, so all memory needed for packet
    //  * buffers can be locked.
    //  */
    // struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    // if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
    //     fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
    //     exit(EXIT_FAILURE);
    // }

    // /* Create AF_XDP socket */
    // struct xsk_socket_info* xsk_socket;
    // xsk_socket = init_xsk_socket(&cfg, xsk_map_fd);
    // if (xsk_socket == NULL) {
    //     lwlog_crit("ERROR: Can't create xsk socket \"%s\"", strerror(errno));
    //     exit(EXIT_FAILURE);
    // }

    // /* Start statistics thread (non-blocking) */
    // pthread_t stats_poll_thread;

    // struct poll_arg poll_arg = {
    //     .xsk = xsk_socket,
    //     .global_exit = &global_exit,
    // };

    // err = pthread_create(&stats_poll_thread, NULL, stats_poll, &poll_arg);
    // if (err) {
    //     fprintf(stderr,
    //             "ERROR: Failed creating statistics thread "
    //             "\"%s\"\n",
    //             strerror(errno));
    //     exit(EXIT_FAILURE);
    // }

    /* Start socket (Polling and non-blocking) */
    pthread_t socket_thread;
    err = pthread_create(&socket_thread, NULL, tcp_server_nonblocking, NULL);
    if (err) {
        fprintf(stderr,
                "ERROR: Failed creating socket thread "
                "\"%s\"\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Start receiving (Blocking)*/
    // rx_and_process(&cfg, xsk_socket, &global_exit);
    // lwlog_info("Exited from poll loop");

    /* Wait for threads to finish */
    // pthread_join(stats_poll_thread, NULL);
    pthread_join(socket_thread, NULL);

    /* Cleanup */
    // xsk_socket__delete(xsk_socket->xsk);
    lwlog_info("XSK socket deleted");
    // err = xsk_umem__delete(xsk_socket->umem->umem);
    // if (err) {
    //     lwlog_crit("ERROR: Can't destroy umem \"%s\"", strerror(errno));
    //     exit(EXIT_FAILURE);
    // }
    // lwlog_info("UMEM destroyed");

    return 0;
}

void exit_application(int signal) {
    int err;

    cfg.unload_all = true;
    global_exit = true;
    err = do_unload(&cfg);
    if (err) {
        lwlog_err("Couldn't detach XDP program on iface '%s' : (%d)", cfg.ifname, err);
    }

    lwlog_info("Exiting XDP Daemon");
    global_exit = true;
}

void* tcp_server_nonblocking() {
    int v_true = 1;
    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = PORT,
        .server = true,
        .nonblocking = true,
        .sockopts =
            {
                {SO_BROADCAST, &v_true, sizeof(v_true)},
            },

    };

    socket99_result res;
    if (!socket99_open(&cfg, &res)) {
        handle_error("socket99_open failed");
        return NULL;
    }

    struct pollfd fds[2] = {{.fd = res.fd, .events = POLLIN}};

    while (!global_exit) {
        lwlog_info("Waiting for client");
        int nready = poll(fds, 1, 3000);  // Timeout set to 3000ms
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

        if (strcmp(buf, "getxskmapfd") == 0) {
            lwlog_info("Sending xsk_map_fd: %d", xsk_map_fd);
            char xsk_map_fd_str[10];
            sprintf(xsk_map_fd_str, "%d", xsk_map_fd);
            send(client_fd, xsk_map_fd_str, strlen(xsk_map_fd_str), 0);
        } else {
            lwlog_info("Unknown command: %s", buf);
        }

    } else if (received == 0) {
        lwlog_info("Client disconnected");
    } else {
        handle_error("recv failed");
    }
}

void handle_error(const char* msg) {
    lwlog_err("%s: %s", msg, strerror(errno));
    errno = 0;
}