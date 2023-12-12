#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>

#include "libxsk.h"
#include "lwlog.h"

#define CLOCK_MONOTONIC 1
#define NANOSEC_PER_SEC 1000000000 /* 10^9 */
static uint64_t gettime(void) {
    struct timespec t;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &t);
    if (res < 0) {
        lwlog_crit("Error with gettimeofday! (%i)\n", res);
        exit(EXIT_FAIL);
    }
    return (uint64_t)t.tv_sec * NANOSEC_PER_SEC + t.tv_nsec;
}

static double calc_period(struct stats_record* r, struct stats_record* p) {
    double period_ = 0;
    __u64 period = 0;

    period = r->timestamp - p->timestamp;
    if (period > 0)
        period_ = ((double)period / NANOSEC_PER_SEC);

    return period_;
}

static void stats_print(struct stats_record* stats_rec, struct stats_record* stats_prev) {
    uint64_t packets, bytes;
    double period;
    double pps; /* packets per sec */
    double bps; /* bits per sec */

    char* fmt =
        "%-12s %'11lld pkts (%'10.0f pps)"
        " %'11lld Kbytes (%'6.0f Mbits/s)"
        " period:%f\n";

    period = calc_period(stats_rec, stats_prev);
    if (period == 0)
        period = 1;

    packets = stats_rec->rx_packets - stats_prev->rx_packets;
    bytes = stats_rec->rx_bytes - stats_prev->rx_bytes;

    if (packets != 0 || bytes != 0) {
        pps = packets / period;
        bps = (bytes * 8) / period / 1000000;
        printf(fmt, "AF_XDP RX:", stats_rec->rx_packets, pps, stats_rec->rx_bytes / 1000, bps, period);
    }

    packets = stats_rec->tx_packets - stats_prev->tx_packets;
    bytes = stats_rec->tx_bytes - stats_prev->tx_bytes;

    if (packets != 0 || bytes != 0) {
        pps = packets / period;
        bps = (bytes * 8) / period / 1000000;
        printf(fmt, "       TX:", stats_rec->tx_packets, pps, stats_rec->tx_bytes / 1000, bps, period);
        printf("\n");
    }
    printf(".");
}

void* stats_poll(void* arg) {
    unsigned int interval = 2;
    struct poll_arg* poll_arg = arg;
    struct xsk_socket_info* xsk = poll_arg->xsk;
    volatile bool* global_exit = poll_arg->global_exit;
    static struct stats_record previous_stats = {0};

    previous_stats.timestamp = gettime();

    /* Trick to pretty printf with thousands separators use %' */
    setlocale(LC_NUMERIC, "en_US");

    while (!*global_exit) {
        sleep(interval);
        xsk->stats.timestamp = gettime();
        stats_print(&xsk->stats, &previous_stats);
        previous_stats = xsk->stats;
    }

    lwlog_info("Exiting stats thread");

    return NULL;
}