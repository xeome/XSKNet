#include <stdlib.h>
#include <signal.h>

#include <net/if.h>
#include <sys/resource.h>
#include <pthread.h>

#include "libxsk.h"
#include "lwlog.h"
#include "flags.h"
#include "xdp_user.h"

static bool global_exit;

struct config* cfg = NULL;

struct tx_if egress = {
    .ifindex = -1,
    .mac = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static const char* __doc__ = "AF_XDP kernel bypass example, User App\n";

void sigint_handler(int signal) {
    global_exit = true;
    lwlog_info("Exiting XDP Daemon");
}

int main(int argc, char** argv) {
    /* Cmdline options can change progname */
    cfg = malloc(sizeof(struct config));
    if (cfg == NULL) {
        lwlog_err("ERROR: Failed to allocate memory for config");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    init_empty_config(cfg);

    lwlog_info("Starting XDP User client");
    parse_cmdline_args(argc, argv, long_options, cfg, __doc__, true);

    /* Request veth creation and XDP program loading from daemon */
    request_port(cfg->ifname);
    set_memory_limit();

    egress.ifindex = if_nametoindex(phy_ifname);

    /* Create AF_XDP socket */
    struct xsk_socket_info* xsk_socket = init_xsk_socket(cfg);
    if (xsk_socket == NULL) {
        lwlog_crit("ERROR: Can't create xsk socket \"%s\"", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Start statistics thread (non-blocking) */
    pthread_t stats_poll_thread;
    struct poll_arg poll_arg = {
        .xsk = xsk_socket,
        .global_exit = &global_exit,
    };
    const int err = pthread_create(&stats_poll_thread, NULL, stats_poll, &poll_arg);
    if (err) {
        lwlog_crit("ERROR: Failed creating stats thread \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Start receiving (Blocking)*/
    rx_and_process(cfg, xsk_socket, &global_exit, &egress);
    lwlog_info("Exited from poll loop");

    /* Wait for threads to finish */
    lwlog_info("Waiting for threads to finish");
    pthread_join(stats_poll_thread, NULL);

    /* Cleanup */
    cleanup(xsk_socket);

    lwlog_info("Exiting XDP User client");
    return 0;
}

void request_port(char* ifname) {
    if (!ifname) {
        lwlog_err("ERROR: Invalid interface name");
        exit(EXIT_FAILURE);
    }

    char msg[1024];
    const int ret = snprintf(msg, sizeof(msg), "create_port %s", ifname);
    if (ret < 0 || ret >= sizeof(msg)) {
        lwlog_err("ERROR: Failed to format message");
        exit(EXIT_FAILURE);
    }

    const char* response = send_to_daemon(msg);
    if (response == NULL) {
        lwlog_err("ERROR: Failed to send message to daemon");
        exit(EXIT_FAILURE);
    }
}

void request_port_deletion(char* ifname) {
    if (!ifname) {
        lwlog_err("ERROR: Invalid interface name");
        exit(EXIT_FAILURE);
    }

    char msg[1024];
    const int ret = snprintf(msg, sizeof(msg), "delete_port %s", ifname);
    if (ret < 0 || ret >= sizeof(msg)) {
        lwlog_err("ERROR: Failed to format message");
        exit(EXIT_FAILURE);
    }

    const char* response = send_to_daemon(msg);
    if (response == NULL) {
        lwlog_err("ERROR: Failed to send message to daemon");
        exit(EXIT_FAILURE);
    }
}

void cleanup(const struct xsk_socket_info* xsk_socket) {
    xsk_socket__delete(xsk_socket->xsk);
    lwlog_info("XSK socket deleted");

    const int err = xsk_umem__delete(xsk_socket->umem->umem);
    if (err) {
        lwlog_crit("ERROR: Can't destroy umem \"%s\"", strerror(errno));
        exit(EXIT_FAILURE);
    }
    lwlog_info("UMEM destroyed");

    // remove last 5 characters from ifname to get the veth name (test_peer -> test)
    char* veth_name = calloc(1, IFNAMSIZ);
    snprintf(veth_name, IFNAMSIZ, "%.*s", (int)strlen(cfg->ifname) - 5, cfg->ifname);

    request_port_deletion(veth_name);
}

/**
 * Allow unlimited locking of memory, so all memory needed for packet
 * buffers can be locked.
 */
static void set_memory_limit() {
    const struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        lwlog_err("ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
