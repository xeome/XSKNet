#include <errno.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/resource.h>
#include <linux/if_ether.h>

#include "veth_list.h"
#include "xdp_utils.h"
#include "xsk_utils.h"
#include "xsk_receive.h"
#include "lwlog.h"

void set_memory_limit() {
    const struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        lwlog_err("ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void init_iface(struct egress_sock* egress, const char* phy_ifname) {
    egress->addr = calloc(1, sizeof(*egress->addr));

    get_mac_address(egress->addr->sll_addr, phy_ifname);
    egress->addr->sll_halen = ETH_ALEN;
    egress->addr->sll_ifindex = if_nametoindex(phy_ifname);
}

static struct xsk_umem_info* configure_xsk_umem(void* buffer, uint64_t size) {
    struct xsk_umem_info* umem = calloc(1, sizeof(*umem));
    if (!umem)
        return NULL;

    const int ret = xsk_umem__create(&umem->umem, buffer, size, &umem->fq, &umem->cq, NULL);
    if (ret) {
        errno = -ret;
        return NULL;
    }

    umem->buffer = buffer;
    return umem;
}

static uint64_t xsk_alloc_umem_frame(struct xsk_socket_info* xsk) {
    if (xsk->umem_frame_free == 0)
        return INVALID_UMEM_FRAME;

    const uint64_t frame = xsk->umem_frame_addr[--xsk->umem_frame_free];
    xsk->umem_frame_addr[xsk->umem_frame_free] = INVALID_UMEM_FRAME;
    return frame;
}

static struct xsk_socket_info* xsk_configure_socket(const char* ifname, struct xsk_umem_info* umem) {
    struct xsk_socket_config xsk_cfg;
    struct bpf_map_info info = {0};
    uint32_t idx;
    int i;
    int ifindex = if_nametoindex(ifname);

    struct xsk_socket_info* xsk_info = calloc(1, sizeof(*xsk_info));
    if (!xsk_info)
        return NULL;

    xsk_info->umem = umem;
    xsk_cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    xsk_cfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;
    xsk_cfg.xdp_flags = 0;
    xsk_cfg.bind_flags = 0;
    xsk_cfg.libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;
    lwlog_info("Creating AF_XDP socket on %s ifindex %d", ifname, ifindex);
    /* Create the AF_XDP socket */
    int ret = xsk_socket__create(&xsk_info->xsk, ifname, 0, umem->umem, &xsk_info->rx, &xsk_info->tx, &xsk_cfg);
    if (ret) {
        lwlog_crit("ERROR: Can't create xsk socket \"%s\"", strerror(errno));
        goto error_exit;
    }

    char pin_dir[PATH_MAX];
    /* Use the --dev name as subdir for finding pinned maps */
    const int len = snprintf(pin_dir, PATH_MAX, "%s/%s", pin_basedir, ifname);
    if (len < 0) {
        fprintf(stderr, "ERR: creating pin dirname\n");
        return NULL;
    }

    /* Get xsk_map fd from pinned map */

    const int xsk_map_fd = open_bpf_map_file(pin_dir, "xsks_map", &info);
    if (xsk_map_fd < 0) {
        lwlog_crit("ERROR: Can't open xskmap \"%s\"", strerror(errno));
        goto error_exit;
    }

    /* Update the xskmap with the xsk socket fd */
    ret = xsk_socket__update_xskmap(xsk_info->xsk, xsk_map_fd);
    if (ret) {
        lwlog_crit("ERROR: Can't update xskmap \"%s\" with fd %d", strerror(errno), xsk_map_fd);
        goto error_exit;
    }

    /* Initialize umem frame allocation */
    for (i = 0; i < NUM_FRAMES; i++)
        xsk_info->umem_frame_addr[i] = i * FRAME_SIZE;

    xsk_info->umem_frame_free = NUM_FRAMES;

    /* Stuff the receive path with buffers, we assume we have enough */
    ret = xsk_ring_prod__reserve(&xsk_info->umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS, &idx);

    if (ret != XSK_RING_PROD__DEFAULT_NUM_DESCS) {
        lwlog_crit("ERROR: Can't reserve enough space for fill queue \"%s\"", strerror(errno));
        goto error_exit;
    }

    for (i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i++)
        *xsk_ring_prod__fill_addr(&xsk_info->umem->fq, idx++) = xsk_alloc_umem_frame(xsk_info);

    xsk_ring_prod__submit(&xsk_info->umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS);

    return xsk_info;

error_exit:
    errno = -ret;
    return NULL;
}

struct xsk_socket_info* init_xsk_socket(const char* prefix) {
    void* buffer;

    const uint64_t size = NUM_FRAMES * FRAME_SIZE;

    /* Allocate memory for NUM_FRAMES of the default XDP frame size */
    const int ret = posix_memalign(&buffer, getpagesize(), size);
    if (ret) {
        errno = -ret;
        lwlog_crit("ERROR: Can't allocate buffer memory: %s", strerror(errno));
        exit(EXIT_FAIL_MEM);
    }

    /* Initialize shared packet_buffer for umem usage */
    struct xsk_umem_info* umem = configure_xsk_umem(buffer, size);
    if (umem == NULL) {
        errno = -ret;
        lwlog_crit("ERROR: Can't create umem \"%s\"", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char* ifname;
    ifname = calloc(1, IF_NAMESIZE);
    snprintf(ifname, IF_NAMESIZE, "%s_inner", prefix);

    struct xsk_socket_info* xsk = xsk_configure_socket(ifname, umem);

    free(ifname);

    return xsk;
}