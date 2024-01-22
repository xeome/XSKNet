#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "args.h"
#include "signal_handler.h"
#include "lwlog.h"
#include "socket.h"
#include "socket_cmds.h"
#include "xdp_utils.h"
#include "veth_list.h"

volatile sig_atomic_t global_exit_flag = 0;

options_t opts;

void exit_daemon() {
    global_exit_flag = 1;

    lwlog_info("Waiting for socket thread to exit");
    pthread_join(socket_thread, NULL);  // its running socket_server_thread_func

    lwlog_info("Unloading XDP from wlan0");
    int err = unload_xdp_from_ifname(opts.dev);
    if (err != EXIT_OK) {
        lwlog_crit("unload_xdp_from_ifname: %s", strerror(err));
    }

    unload_list();

    exit(EXIT_SUCCESS);
}

/*
 * Signal handler for SIGINT
 */
static void daemon_sigint_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Interrupted by user\n" NONE);
    exit_daemon();
}

/*
 * Signal handler for SIGTERM
 */
static void daemon_sigterm_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Terminated\n" NONE);
    exit_daemon();
}

/*
 * Initializes the signal handlers
 */
void daemon_signal_init() {
    signal(SIGINT, daemon_sigint_handler);
    signal(SIGTERM, daemon_sigterm_handler);
}

void exit_client() {
    global_exit_flag = 1;

    remove_port(opts.dev);

    exit(EXIT_SUCCESS);
}
static void client_sigint_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Interrupted by user\n" NONE);
    exit_client();
}

static void client_sigterm_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Terminated\n" NONE);
    exit_client();
}

void client_signal_init() {
    signal(SIGINT, client_sigint_handler);
    signal(SIGTERM, client_sigterm_handler);
}