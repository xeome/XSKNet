#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>
#include <sys/resource.h>
#include <pthread.h>

#include "xdp_socket.h"
#include "socket_stats.h"
#include "xdp_receive.h"
#include "defs.h"
#include "lwlog.h"
#include "socket99.h"
#include "xdp_user.h"
#include "daemon_api.h"

int xsk_map_fd;
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
    char* msg = "create_port test";
    char* res = send_to_daemon(msg);
    if (res == NULL) {
        lwlog_err("ERROR: Failed to send message to daemon");
        exit(EXIT_FAILURE);
    }

    /* Allow unlimited locking of memory, so all memory needed for packet
     * buffers can be locked.
     */
    struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        lwlog_err("ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

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

    return 0;
}
