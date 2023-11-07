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

int xsk_map_fd;
static bool global_exit;

static struct config cfg = {
    .ifindex = -1,
    .unload_all = true,
};

int main(int argc, char** argv) {
    int err;
    socket99_config socket_cfg = {
        .host = "127.0.0.1",
        .port = 8080,
        .nonblocking = true,
    };

    socket99_result res;
    bool ok = socket99_open(&socket_cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }

    struct pollfd fds[1] = {
        {res.fd, POLLOUT, 0},
    };
    const int TIMEOUT_MSEC = 10 * 1000;

    int pres = poll(fds, 1, TIMEOUT_MSEC);
    if (pres == -1) {
        printf("poll: %s\n", strerror(errno));
        errno = 0;
        return false;
    }

    if (pres != 1) {
        lwlog_err("poll: pres != 1");
        return false;
    }

    const char* msg = "getxskmapfd";
    size_t msg_size = strlen(msg);
    if (fds[0].revents & POLLOUT) {
        ssize_t sent = send(res.fd, msg, msg_size, 0);
        lwlog_info("sent: %ld bytes", sent);

        char buffer[1024] = {0};
        ssize_t valread;
        struct pollfd pfd = {.fd = res.fd, .events = POLLIN};  // Set up pollfd structure for polling

        do {
            int poll_res = poll(&pfd, 1, -1);  // Poll indefinitely until data is available to read
            if (poll_res == -1) {
                lwlog_err("poll: %s", strerror(errno));
                return false;
            }

            if (pfd.revents & POLLIN) {  // If data is available to read
                valread = recv(res.fd, buffer, 1024, 0);
                if (valread == -1) {
                    lwlog_err("recv: %s", strerror(errno));
                    return false;
                }
            }
        } while (valread == -1);

        lwlog_info("received: %s", buffer);
        xsk_map_fd = atoi(buffer);
    }

    if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP) {
        lwlog_err("poll: POLLERR or POLLHUP");
    }
    close(res.fd);

    /* Allow unlimited locking of memory, so all memory needed for packet
     * buffers can be locked.
     */
    struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
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
