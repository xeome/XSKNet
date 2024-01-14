#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "signal_handler.h"
#include "lwlog.h"
#include "socket.h"

volatile sig_atomic_t global_exit_flag = 0;

void exit_daemon() {
    global_exit_flag = 1;

    lwlog_info("Waiting for socket thread to exit");
    pthread_join(socket_thread, NULL);  // its running socket_server_thread_func
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
