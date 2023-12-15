#include <poll.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <linux/icmp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "libxsk.h"
#include "lwlog.h"

/**
 * @brief Allocates a frame from the user memory (umem).
 *
 * This function attempts to allocate a frame from the umem associated with the given xsk_socket_info structure.
 * If no frames are available, it returns INVALID_UMEM_FRAME.
 *
 * @return The address of the allocated frame, or INVALID_UMEM_FRAME if no frames could be allocated.
 */
static uint64_t xsk_alloc_umem_frame(struct xsk_socket_info* xsk) {
    if (xsk->umem_frame_free == 0)
        return INVALID_UMEM_FRAME;

    // Decrement the free frame count and return the address of the frame
    const uint64_t frame = xsk->umem_frame_addr[--xsk->umem_frame_free];
    // Mark the frame as invalid
    xsk->umem_frame_addr[xsk->umem_frame_free] = INVALID_UMEM_FRAME;
    return frame;
}

static void xsk_free_umem_frame(struct xsk_socket_info* xsk, uint64_t frame) {
    assert(xsk->umem_frame_free < NUM_FRAMES);

    // Increment the free frame count and store the address of the frame in the free frame array
    xsk->umem_frame_addr[xsk->umem_frame_free++] = frame;
}

static uint64_t xsk_umem_free_frames(const struct xsk_socket_info* xsk) {
    return xsk->umem_frame_free;
}

static void complete_tx(struct xsk_socket_info* xsk) {
    uint32_t idx_cq;

    if (!xsk->outstanding_tx)
        return;

    /* Non-blocking wakeup of kernel for completions */
    sendto(xsk_socket__fd(xsk->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);

    /* Collect/free completed TX buffers */
    const unsigned int completed = xsk_ring_cons__peek(&xsk->umem->cq, XSK_RING_CONS__DEFAULT_NUM_DESCS, &idx_cq);

    if (completed <= 0)
        return;

    /* For each completed transmission, free the corresponding user memory frame. */
    for (int i = 0; i < completed; i++)
        xsk_free_umem_frame(xsk, *xsk_ring_cons__comp_addr(&xsk->umem->cq, idx_cq++));

    /* Release the completed transmissions from the completion queue. */
    xsk_ring_cons__release(&xsk->umem->cq, completed);
    xsk->outstanding_tx -= completed < xsk->outstanding_tx ? completed : xsk->outstanding_tx;
}

static inline void csum_replace2(uint16_t* sum, const uint16_t old, const uint16_t new) {
    uint16_t csum = ~*sum;  // 1's complement of the checksum (flip all the bits)

    csum += ~old;                   // Subtract the old value from the checksum
    csum += csum < (uint16_t)~old;  // If the subtraction overflowed, add 1 to the checksum

    csum += new;                    // Add the new value to the checksum
    csum += csum < (uint16_t) new;  // If the addition overflowed, add 1 to the checksum

    *sum = ~csum;  // 1's complement of the checksum
}

static bool process_packet(const struct xsk_socket_info* xsk, uint64_t addr, uint32_t len, const struct tx_if* egress) {
    uint8_t* pkt = xsk_umem__get_data(xsk->umem->buffer, addr);

    errno = 0;
    int ret;
    struct in_addr tmp_ip;
    struct ethhdr* eth = (struct ethhdr*)pkt;
    struct iphdr* ipv4 = (struct iphdr*)(eth + 1);
    struct icmphdr* icmp = (struct icmphdr*)(ipv4 + 1);

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
    uint8_t tmp_mac[ETH_ALEN];
    memcpy(tmp_mac, eth->h_dest, ETH_ALEN);
    memcpy(eth->h_dest, eth->h_source, ETH_ALEN);
    memcpy(eth->h_source, tmp_mac, ETH_ALEN);

    memcpy(&tmp_ip, &ipv4->saddr, sizeof(tmp_ip));
    memcpy(&ipv4->saddr, &ipv4->daddr, sizeof(tmp_ip));
    memcpy(&ipv4->daddr, &tmp_ip, sizeof(tmp_ip));
    icmp->type = ICMP_ECHOREPLY;
    csum_replace2(&icmp->checksum, ICMP_ECHO, ICMP_ECHOREPLY);

    lwlog_info("Transmitting ICMPv4 echo reply");
    lwlog_info("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_source[0], eth->h_source[1], eth->h_source[2],
               eth->h_source[3], eth->h_source[4], eth->h_source[5]);
    lwlog_info("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3],
               eth->h_dest[4], eth->h_dest[5]);
    lwlog_info("Source IP: %s", inet_ntoa(*(struct in_addr*)&ipv4->saddr));
    lwlog_info("Dest IP: %s", inet_ntoa(*(struct in_addr*)&ipv4->daddr));

    // Open raw socket
    int sockfd;
    struct sockaddr_ll socket_address;

    /* Open RAW socket to send on */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
        lwlog_err("ERROR: Failed to open raw socket");
        return false;
    }

    socket_address.sll_ifindex = egress->ifindex;
    socket_address.sll_halen = ETH_ALEN;
    socket_address.sll_addr[0] = egress->mac[0];
    socket_address.sll_addr[1] = egress->mac[1];
    socket_address.sll_addr[2] = egress->mac[2];
    socket_address.sll_addr[3] = egress->mac[3];
    socket_address.sll_addr[4] = egress->mac[4];
    socket_address.sll_addr[5] = egress->mac[5];

    /* Send packet */
    if ((ret = sendto(sockfd, pkt, len, 0, (struct sockaddr*)&socket_address, sizeof(socket_address))) == -1) {
        lwlog_err("ERROR: Failed to send packet");
        return false;
    }

    /* Here we send the packet out of the receive port. Note that
     * we allocate one entry and schedule it. Your design would be
     * faster if you do batch processing/transmission */
    // ret = xsk_ring_prod__reserve(&xsk->tx, 1, &tx_idx);
    // if (ret != 1) {
    //     lwlog_warning("Dropping packet due to lack of transmit slots");
    //     return false;
    // }

    // xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->addr = addr;
    // xsk_ring_prod__tx_desc(&xsk->tx, tx_idx)->len = len;
    // xsk_ring_prod__submit(&xsk->tx, 1);
    // xsk->outstanding_tx++;

    // xsk->stats.tx_bytes += len;
    // xsk->stats.tx_packets++;
    // Do not transmit
    return false;
}

static void handle_receive_packets(struct xsk_socket_info* xsk, const struct tx_if* egress) {
    unsigned int i;
    uint32_t idx_rx = 0, idx_fq = 0;

    const unsigned int rcvd = xsk_ring_cons__peek(&xsk->rx, RX_BATCH_SIZE, &idx_rx);
    if (!rcvd)
        return;

    /* Stuff the ring with as much frames as possible */
    const unsigned int stock_frames = xsk_prod_nb_free(&xsk->umem->fq, xsk_umem_free_frames(xsk));

    if (stock_frames > 0) {
        int ret = xsk_ring_prod__reserve(&xsk->umem->fq, stock_frames, &idx_fq);

        /* This should not happen, but just in case
         * Wait until we can reserve enough space in the fill queue
         */
        while (ret != stock_frames)
            ret = xsk_ring_prod__reserve(&xsk->umem->fq, rcvd, &idx_fq);

        for (i = 0; i < stock_frames; i++)
            *xsk_ring_prod__fill_addr(&xsk->umem->fq, idx_fq++) = xsk_alloc_umem_frame(xsk);

        /* Finally, tell the kernel that it can start writing packets into the rx ring */
        xsk_ring_prod__submit(&xsk->umem->fq, stock_frames);
    }

    /* Process received packets */
    for (i = 0; i < rcvd; i++) {
        /* Get the address of the frame from the rx ring */
        const uint64_t addr = xsk_ring_cons__rx_desc(&xsk->rx, idx_rx)->addr;
        const uint32_t len = xsk_ring_cons__rx_desc(&xsk->rx, idx_rx++)->len;

        /* If the packet was not processed correctly or does not need to be transmitted, free the frame */
        if (!process_packet(xsk, addr, len, egress))
            xsk_free_umem_frame(xsk, addr);

        xsk->stats.rx_bytes += len;
    }

    xsk_ring_cons__release(&xsk->rx, rcvd);
    xsk->stats.rx_packets += rcvd;

    /* Do we need to wake up the kernel for transmission */
    complete_tx(xsk);
}

void get_mac_address(unsigned char* mac_addr, const char* ifname) {
    struct ifreq ifr;
    if (ifname == NULL) {
        lwlog_err("ERROR: Couldn't get interface name from index");
        exit(EXIT_FAILURE);
    }

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        lwlog_err("ERROR: Couldn't create socket");
        exit(EXIT_FAILURE);
    }

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
        lwlog_err("ERROR: Couldn't get MAC address");
        exit(EXIT_FAILURE);
    }

    close(fd);

    memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
}

void rx_and_process(struct config* cfg, struct xsk_socket_info* xsk_socket, const bool* global_exit, struct tx_if* egress) {
    struct pollfd fds[2];

    get_mac_address(egress->mac, phy_ifname);

    memset(fds, 0, sizeof(fds));
    fds[0].fd = xsk_socket__fd(xsk_socket->xsk);
    fds[0].events = POLLIN;

    while (!*global_exit) {
        const int nfds = 1;
        const int ret = poll(fds, nfds, -1);
        if (ret <= 0 || ret > 1)
            continue;
        handle_receive_packets(xsk_socket, egress);
    }
}