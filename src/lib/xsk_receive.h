#include <linux/if_packet.h>
#include <stdbool.h>

#include "xsk_utils.h"

struct egress_sock {
    int sockfd;
    struct sockaddr_ll* addr;
};
void init_iface(struct egress_sock* egress, const char* phy_ifname);

void get_mac_address(unsigned char* mac_addr, const char* ifname);
void rx_and_process(struct xsk_socket_info* xsk_socket, const int* global_exit, struct egress_sock* egress);