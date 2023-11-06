#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <bpf/bpf.h>
#include <xdp/libxdp.h>
#include <xdp/xsk.h>

#include "defs.h"
#include "lwlog.h"
#include "xdp_socket.h"

static struct xsk_umem_info* configure_xsk_umem(void* buffer, uint64_t size) {
    struct xsk_umem_info* umem;
    int ret;

    umem = calloc(1, sizeof(*umem));
    if (!umem)
        return NULL;

    ret = xsk_umem__create(&umem->umem, buffer, size, &umem->fq, &umem->cq, NULL);
    if (ret) {
        errno = -ret;
        return NULL;
    }

    umem->buffer = buffer;
    return umem;
}

static uint64_t xsk_alloc_umem_frame(struct xsk_socket_info* xsk) {
    uint64_t frame;
    if (xsk->umem_frame_free == 0)
        return INVALID_UMEM_FRAME;

    frame = xsk->umem_frame_addr[--xsk->umem_frame_free];
    xsk->umem_frame_addr[xsk->umem_frame_free] = INVALID_UMEM_FRAME;
    return frame;
}

static struct xsk_socket_info* xsk_configure_socket(struct config* cfg, struct xsk_umem_info* umem, int xsk_map_fd) {
    struct xsk_socket_config xsk_cfg;
    struct xsk_socket_info* xsk_info;
    uint32_t idx;
    int i;
    int ret;

    xsk_info = calloc(1, sizeof(*xsk_info));
    if (!xsk_info)
        return NULL;

    xsk_info->umem = umem;
    xsk_cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    xsk_cfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;
    xsk_cfg.xdp_flags = cfg->xdp_flags;
    xsk_cfg.bind_flags = cfg->xsk_bind_flags;
    xsk_cfg.libbpf_flags = XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;
    ret = xsk_socket__create(&xsk_info->xsk, cfg->ifname, cfg->xsk_if_queue, umem->umem, &xsk_info->rx, &xsk_info->tx, &xsk_cfg);
    if (ret)
        goto error_exit;

    ret = xsk_socket__update_xskmap(xsk_info->xsk, xsk_map_fd);
    if (ret)
        goto error_exit;

    /* Initialize umem frame allocation */
    for (i = 0; i < NUM_FRAMES; i++)
        xsk_info->umem_frame_addr[i] = i * FRAME_SIZE;

    xsk_info->umem_frame_free = NUM_FRAMES;

    /* Stuff the receive path with buffers, we assume we have enough */
    ret = xsk_ring_prod__reserve(&xsk_info->umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS, &idx);

    if (ret != XSK_RING_PROD__DEFAULT_NUM_DESCS)
        goto error_exit;

    for (i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i++)
        *xsk_ring_prod__fill_addr(&xsk_info->umem->fq, idx++) = xsk_alloc_umem_frame(xsk_info);

    xsk_ring_prod__submit(&xsk_info->umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS);

    return xsk_info;

error_exit:
    errno = -ret;
    return NULL;
}

struct xsk_socket_info* init_xsk_socket(struct config* cfg, int xsk_map_fd) {
    int ret;
    struct xsk_umem_info* umem;
    struct xsk_socket_info* xsk;
    void* buffer;
    uint64_t size;

    size = NUM_FRAMES * FRAME_SIZE;

    ret = posix_memalign(&buffer, getpagesize(), size);
    if (ret) {
        errno = -ret;
        lwlog_crit("ERROR: Can't allocate buffer memory: %s", strerror(errno));
        exit(EXIT_FAIL_MEM);
    }

    umem = configure_xsk_umem(buffer, size);
    if (umem == NULL) {
        errno = -ret;
        lwlog_crit("ERROR: Can't create umem \"%s\"", strerror(errno));
        exit(EXIT_FAILURE);
    }

    xsk = xsk_configure_socket(cfg, umem, xsk_map_fd);
    return xsk;
}