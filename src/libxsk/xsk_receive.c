#include <poll.h>
#include <assert.h>

#include <arpa/inet.h>
#include <linux/icmp.h>
#include <linux/if_ether.h>
#include <linux/ip.h>

#include "libxsk.h"
#include "lwlog.h"

static uint64_t xsk_alloc_umem_frame(struct xsk_socket_info* xsk) {
    uint64_t frame;
    if (xsk->umem_frame_free == 0)
        return INVALID_UMEM_FRAME;

    frame = xsk->umem_frame_addr[--xsk->umem_frame_free];
    xsk->umem_frame_addr[xsk->umem_frame_free] = INVALID_UMEM_FRAME;
    return frame;
}

static void xsk_free_umem_frame(struct xsk_socket_info* xsk, uint64_t frame) {
    assert(xsk->umem_frame_free < NUM_FRAMES);

    xsk->umem_frame_addr[xsk->umem_frame_free++] = frame;
}

static uint64_t xsk_umem_free_frames(struct xsk_socket_info* xsk) {
    return xsk->umem_frame_free;
}

static void complete_tx(struct xsk_socket_info* xsk) {
    unsigned int completed;
    uint32_t idx_cq;

    if (!xsk->outstanding_tx)
        return;

    sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);

    /* Collect/free completed TX buffers */
    completed = xsk_ring_cons__peek(&xsk->umem->cq, XSK_RING_CONS__DEFAULT_NUM_DESCS, &idx_cq);

    if (completed > 0) {
        for (int i = 0; i < completed; i++)
            xsk_free_umem_frame(xsk, *xsk_ring_cons__comp_addr(&xsk->umem->cq, idx_cq++));

        xsk_ring_cons__release(&xsk->umem->cq, completed);
        xsk->outstanding_tx -= completed < xsk->outstanding_tx ? completed : xsk->outstanding_tx;
    }
}

// static inline __sum16 csum16_add(__sum16 csum, __be16 addend) {
//     uint16_t res = (uint16_t)csum;

//     res += (__u16)addend;
//     return (__sum16)(res + (res < (__u16)addend));
// }

// static inline __sum16 csum16_sub(__sum16 csum, __be16 addend) {
//     return csum16_add(csum, ~addend);
// }

// static inline void csum_replace2(__sum16* sum, __be16 old, __be16 new) {
//     *sum = ~csum16_add(csum16_sub(~(*sum), old), new);
// }
static inline void csum_replace2(uint16_t* sum, uint16_t old, uint16_t new) {
    uint16_t csum = ~*sum;  // 1's complement of the checksum (flip all the bits)

    csum += ~old;                   // Subtract the old value from the checksum
    csum += csum < (uint16_t)~old;  // If the subtraction overflowed, add 1 to the checksum

    csum += new;                    // Add the new value to the checksum
    csum += csum < (uint16_t) new;  // If the addition overflowed, add 1 to the checksum

    *sum = ~csum;  // 1's complement of the checksum
}

// ipv4 instead
static bool process_packet(struct xsk_socket_info* xsk, uint64_t addr, uint32_t len) {
    uint8_t* pkt = xsk_umem__get_data(xsk->umem->buffer, addr);

    errno = 0;
    int ret;
    uint32_t tx_idx = 0;
    uint8_t tmp_mac[ETH_ALEN];
    struct in_addr tmp_ip;
    struct ethhdr* eth = (struct ethhdr*)pkt;
    struct iphdr* ipv4 = (struct iphdr*)(eth + 1);
    struct icmphdr* icmp = (struct icmphdr*)(ipv4 + 1);

    // if (len < (sizeof(*eth) + sizeof(*ipv4) + sizeof(*icmp))) {
    //     lwlog_warning("Received too small packet");
    //     return false;
    // }

    if (len < sizeof(*eth)) {
        return false;
    }

    if (len < sizeof(*ipv4)) {
        return false;
    }

    if (len < sizeof(*icmp)) {
        return false;
    }

    if (ntohs(eth->h_proto) != ETH_P_IP) {
        lwlog_warning("Received non-IPv4 packet");
        return false;
    }

    if (ipv4->protocol != IPPROTO_ICMP) {
        lwlog_warning("Received non-ICMPv4 packet");
        return false;
    }
    if (icmp->type != ICMP_ECHO) {
        lwlog_warning("Received non-ICMPv4 echo request");
        return false;
    }

    memcpy(tmp_mac, eth->h_dest, ETH_ALEN);
    memcpy(eth->h_dest, eth->h_source, ETH_ALEN);
    memcpy(eth->h_source, tmp_mac, ETH_ALEN);

    memcpy(&tmp_ip, &ipv4->saddr, sizeof(tmp_ip));
    memcpy(&ipv4->saddr, &ipv4->daddr, sizeof(tmp_ip));
    memcpy(&ipv4->daddr, &tmp_ip, sizeof(tmp_ip));

    icmp->type = ICMP_ECHOREPLY;

    csum_replace2(&icmp->checksum, htons(ICMP_ECHO << 8), htons(ICMP_ECHOREPLY << 8));

    /* Here we sent the packet out of the receive port. Note that
     * we allocate one entry and schedule it. Your design would be
     * faster if you do batch processing/transmission */
    ret = xsk_ring_prod__reserve(&xsk->tx, 1, &tx_idx);
    if (ret != 1) {
        /* No more transmit slots, drop the packet */
        lwlog_warning("Dropping packet due to lack of transmit slots");
        return false;
    }

    xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->addr = addr;
    xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->len = len;
    xsk_ring_prod__submit(&xsk->tx, 1);
    xsk->outstanding_tx++;

    xsk->stats.tx_bytes += len;
    xsk->stats.tx_packets++;
    return true;
}

// static bool process_packet(struct xsk_socket_info* xsk, uint64_t addr, uint32_t len) {
//     uint8_t* pkt = xsk_umem__get_data(xsk->umem->buffer, addr);

//     int ret;
//     uint32_t tx_idx = 0;
//     uint8_t tmp_mac[ETH_ALEN];
//     struct in6_addr tmp_ip;
//     struct ethhdr* eth = (struct ethhdr*)pkt;
//     struct ipv6hdr* ipv6 = (struct ipv6hdr*)(eth + 1);
//     struct icmp6hdr* icmp = (struct icmp6hdr*)(ipv6 + 1);

//     if (ntohs(eth->h_proto) != ETH_P_IPV6) {
//         lwlog_warning("Received non-IPv6 packet");
//         return false;
//     }
//     if (len < (sizeof(*eth) + sizeof(*ipv6) + sizeof(*icmp))) {
//         lwlog_warning("Received too small packet");
//         return false;
//     }
//     if (ipv6->nexthdr != IPPROTO_ICMPV6) {
//         lwlog_warning("Received non-ICMPv6 packet");
//         return false;
//     }
//     if (icmp->icmp6_type != ICMPV6_ECHO_REQUEST) {
//         lwlog_warning("Received non-ICMPv6 echo request");
//         return false;
//     }

//     memcpy(tmp_mac, eth->h_dest, ETH_ALEN);
//     memcpy(eth->h_dest, eth->h_source, ETH_ALEN);
//     memcpy(eth->h_source, tmp_mac, ETH_ALEN);

//     memcpy(&tmp_ip, &ipv6->saddr, sizeof(tmp_ip));
//     memcpy(&ipv6->saddr, &ipv6->daddr, sizeof(tmp_ip));
//     memcpy(&ipv6->daddr, &tmp_ip, sizeof(tmp_ip));

//     icmp->icmp6_type = ICMPV6_ECHO_REPLY;

//     csum_replace2(&icmp->icmp6_cksum, htons(ICMPV6_ECHO_REQUEST << 8), htons(ICMPV6_ECHO_REPLY << 8));

//     /* Here we sent the packet out of the receive port. Note that
//      * we allocate one entry and schedule it. Your design would be
//      * faster if you do batch processing/transmission */
//     ret = xsk_ring_prod__reserve(&xsk->tx, 1, &tx_idx);
//     if (ret != 1) {
//         /* No more transmit slots, drop the packet */
//         lwlog_warning("Dropping packet due to lack of transmit slots");
//         return false;
//     }

//     xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->addr = addr;
//     xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->len = len;
//     xsk_ring_prod__submit(&xsk->tx, 1);
//     xsk->outstanding_tx++;

//     xsk->stats.tx_bytes += len;
//     xsk->stats.tx_packets++;
//     return true;
// }

static void handle_receive_packets(struct xsk_socket_info* xsk) {
    unsigned int rcvd, stock_frames, i;
    uint32_t idx_rx = 0, idx_fq = 0;
    int ret;

    rcvd = xsk_ring_cons__peek(&xsk->rx, RX_BATCH_SIZE, &idx_rx);
    if (!rcvd)
        return;

    /* Stuff the ring with as much frames as possible */
    stock_frames = xsk_prod_nb_free(&xsk->umem->fq, xsk_umem_free_frames(xsk));

    if (stock_frames > 0) {
        ret = xsk_ring_prod__reserve(&xsk->umem->fq, stock_frames, &idx_fq);

        /* This should not happen, but just in case */
        while (ret != stock_frames)
            ret = xsk_ring_prod__reserve(&xsk->umem->fq, rcvd, &idx_fq);

        for (i = 0; i < stock_frames; i++)
            *xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_fq++) = xsk_alloc_umem_frame(xsk);

        xsk_ring_prod__submit(&xsk->umem->fq, stock_frames);
    }

    /* Process received packets */
    for (i = 0; i < rcvd; i++) {
        uint64_t addr = xsk_ring_cons__rx_desc(&xsk->rx, idx_rx)->addr;
        uint32_t len = xsk_ring_cons__rx_desc(&xsk->rx, idx_rx++)->len;

        if (!process_packet(xsk, addr, len))
            xsk_free_umem_frame(xsk, addr);

        xsk->stats.rx_bytes += len;
    }

    xsk_ring_cons__release(&xsk->rx, rcvd);
    xsk->stats.rx_packets += rcvd;

    /* Do we need to wake up the kernel for transmission */
    complete_tx(xsk);
}

void rx_and_process(struct config* cfg, struct xsk_socket_info* xsk_socket, bool* global_exit) {
    struct pollfd fds[2];
    int ret, nfds = 1;

    memset(fds, 0, sizeof(fds));
    fds[0].fd = xsk_socket__fd(xsk_socket->xsk);
    fds[0].events = POLLIN;

    while (!*global_exit) {
        if (cfg->xsk_poll_mode) {
            ret = poll(fds, nfds, -1);
            if (ret <= 0 || ret > 1)
                continue;
        }
        handle_receive_packets(xsk_socket);
    }
}