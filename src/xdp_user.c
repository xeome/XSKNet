#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "xdp_socket.h"
#include "xdp_receive.h"
#include "defs.h"
#include "lwlog.h"
#include "socket99.h"

static bool global_exit;

int main(int argc, char** argv) {
    socket99_config cfg = {
        .host = "127.0.0.1",
        .port = 8080,
        .nonblocking = true,
    };

    socket99_result res;
    bool ok = socket99_open(&cfg, &res);
    if (!ok) {
        socket99_fprintf(stderr, &res);
        return false;
    }
    const char* msg = "hello\n";
    size_t msg_size = strlen(msg);

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

    if (fds[0].revents & POLLOUT) {
        ssize_t sent = send(res.fd, msg, msg_size, 0);
        lwlog_info("sent: %ld", sent);
    }

    if (fds[0].revents & POLLERR || fds[0].revents & POLLHUP) {
        lwlog_err("poll: POLLERR or POLLHUP");
    }
    close(res.fd);
    return 0;
}