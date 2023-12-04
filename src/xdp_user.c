#include <stdlib.h>
#include <signal.h>

#include <net/if.h>
#include <sys/resource.h>
#include <pthread.h>

#include "libxsk.h"
#include "lwlog.h"
#include "flags.h"

static bool global_exit;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

static const char* __doc__ = "AF_XDP kernel bypass example, User App\n";

const struct option_wrapper long_options[] = {
    {{"help", no_argument, NULL, 'h'}, "Show help", false},
    {{"dev", required_argument, NULL, 'd'}, "Operate on device <ifname>", "<ifname>", true},
    {{"poll-mode", no_argument, NULL, 'p'}, "Use the poll() API waiting for packets to arrive"},
    {{"quiet", no_argument, NULL, 'q'}, "Quiet mode (no output)"},
    {{0, 0, NULL, 0}, NULL, false}};

void request_port();
void request_port_deletion();
void sigint_handler(int signal) {
    global_exit = true;
    lwlog_info("Exiting XDP Daemon");
}

int main(int argc, char** argv) {
    int err;
    /* Cmdline options can change progname */
    parse_cmdline_args(argc, argv, long_options, &cfg, __doc__, true);

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    lwlog_info("Starting XDP User client");

    /* Request veth creation and XDP program loading from daemon */
    request_port();

    /* Allow unlimited locking of memory, so all memory needed for packet
     * buffers can be locked.
     */
    struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        lwlog_err("ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char* veth_peer_name = malloc(sizeof(char) * IFNAMSIZ);
    if (veth_peer_name == NULL) {
        lwlog_err("ERROR: Couldn't allocate memory for veth peer name");
        exit(EXIT_FAILURE);
    }
    snprintf(veth_peer_name, IFNAMSIZ, "%s_peer", cfg.ifname);
    char* ifname_orig = strdup(cfg.ifname);
    cfg.ifindex = if_nametoindex(veth_peer_name);
    cfg.ifname = veth_peer_name;

    /* Create AF_XDP socket */
    struct xsk_socket_info* xsk_socket;
    xsk_socket = init_xsk_socket(&cfg);
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

    err = pthread_create(&stats_poll_thread, NULL, stats_poll, &poll_arg);
    if (err) {
        lwlog_crit("ERROR: Failed creating stats thread \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Start receiving (Blocking)*/
    rx_and_process(&cfg, xsk_socket, &global_exit);
    lwlog_info("Exited from poll loop");

    /* Wait for threads to finish */
    lwlog_info("Waiting for threads to finish");
    pthread_join(stats_poll_thread, NULL);

    /* Cleanup */
    xsk_socket__delete(xsk_socket->xsk);
    lwlog_info("XSK socket deleted");
    err = xsk_umem__delete(xsk_socket->umem->umem);
    if (err) {
        lwlog_crit("ERROR: Can't destroy umem \"%s\"", strerror(errno));
        exit(EXIT_FAILURE);
    }
    lwlog_info("UMEM destroyed");

    cfg.ifname = ifname_orig;
    /* Request veth deletion and XDP program unloading from daemon */
    request_port_deletion();

    lwlog_info("Exiting XDP User client");
    return 0;
}

void request_port() {
    char msg[1024];
    int ret = snprintf(msg, sizeof(msg), "create_port %s", cfg.ifname);
    if (ret < 0 || ret >= sizeof(msg)) {
        lwlog_err("ERROR: Failed to format message");
        exit(EXIT_FAILURE);
    }

    char* response = send_to_daemon(msg);
    if (response == NULL) {
        lwlog_err("ERROR: Failed to send message to daemon");
        exit(EXIT_FAILURE);
    }
}

void request_port_deletion() {
    char msg[1024];
    int ret = snprintf(msg, sizeof(msg), "delete_port %s", cfg.ifname);
    if (ret < 0 || ret >= sizeof(msg)) {
        lwlog_err("ERROR: Failed to format message");
        exit(EXIT_FAILURE);
    }

    char* response = send_to_daemon(msg);
    if (response == NULL) {
        lwlog_err("ERROR: Failed to send message to daemon");
        exit(EXIT_FAILURE);
    }
}