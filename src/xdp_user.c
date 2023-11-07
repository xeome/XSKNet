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

int xsk_map_fd;
static bool global_exit;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

int main(int argc, char** argv) {
    int err;

    /* get xsk map fd from xdp daemon */
    bool ret = get_map_fd();
    if (!ret) {
        lwlog_err("Failed to get map fd");
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
    xsk_socket = init_xsk_socket(&cfg, xsk_map_fd);
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
        fprintf(stderr,
                "ERROR: Failed creating statistics thread "
                "\"%s\"\n",
                strerror(errno));
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

bool get_map_fd() {
    socket99_config socket_cfg = {
        .host = "127.0.0.1",
        .port = 8080,
    };

    socket99_result res;
    if (!socket99_open(&socket_cfg, &res)) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    const int TIMEOUT_MSEC = 10 * 1000;

    struct pollfd fd = {.fd = res.fd, .events = POLLOUT};
    if (poll(&fd, 1, TIMEOUT_MSEC) <= 0) {
        lwlog_err("poll: %s", strerror(errno));
        close(res.fd);
        errno = 0;
        return false;
    }

    if (fd.revents & POLLOUT) {
        const char* msg = "getxskmapfd";
        ssize_t sent = send(res.fd, msg, strlen(msg), 0);
        lwlog_info("sent: %ld bytes", sent);
        if (sent == -1) {
            lwlog_err("send: %s", strerror(errno));
            close(res.fd);
            return false;
        }

        fd.events = POLLIN;
        char buffer[1024] = {0};
        ssize_t valread = recv(res.fd, buffer, sizeof(buffer) - 1, 0);
        if (valread == -1) {
            lwlog_err("recv: %s", strerror(errno));
            close(res.fd);
            return false;
        }

        lwlog_info("received: %s", buffer);
        xsk_map_fd = atoi(buffer);
    } else if (fd.revents & (POLLERR | POLLHUP)) {
        lwlog_err("poll: POLLERR or POLLHUP");
        close(res.fd);
        return false;
    }

    close(res.fd);
    return true;
}