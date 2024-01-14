#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "signal_handler.h"
#include "lwlog.h"
#include "socket.h"
#include "xdp_utils.h"

volatile sig_atomic_t global_exit_flag = 0;

void exit_daemon() {
    global_exit_flag = 1;

    lwlog_info("Waiting for socket thread to exit");
    pthread_join(socket_thread, NULL);  // its running socket_server_thread_func

    lwlog_info("Unloading XDP from wlan0");
    int err = unload_xdp_from_ifname("wlan0");
    if (err != EXIT_OK) {
        lwlog_crit("unload_xdp_from_ifname: %s", strerror(err));
    }

    exit(EXIT_SUCCESS);
}

/*
 * Signal handler for SIGINT
 */
static void sigint_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Interrupted by user\n" NONE);
    exit_daemon();
}

/*
 * Signal handler for SIGTERM
 */
static void sigterm_handler() {
    fprintf(stderr, "\n");
    fprintf(stderr, RED "Terminated\n" NONE);
    exit_daemon();
}

/*
 * Initializes the signal handlers
 */
void signal_init() {
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);
}
