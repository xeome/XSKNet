#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <linux/icmp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <sys/cdefs.h>

/**
 * Main XDP program entry point.
 * This is the entry point for all XDP packets. It redirects packets depending on the arbitrary port number defined in eth data
 * payload. It will redirect packets to different virtual interfaces depending on the port number.
 */

#ifndef memcpy
#define memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))
#endif

struct pkt_meta {
    __u32 port;
};

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __type(key, __u32);
    __type(value, __u32);
    __uint(max_entries, 64);
} xdp_stats_map SEC(".maps");

// devmap
struct {
    __uint(type, BPF_MAP_TYPE_DEVMAP);
    __uint(key_size, sizeof(int));
    __uint(value_size, sizeof(int));
    __uint(max_entries, 64);
} xdp_devmap SEC(".maps");

// static __always_inline int parse_ethhdr(struct ethhdr* eth, void* data_end, struct pkt_meta* pkt_meta) {
//     void* data = (void*)(eth + 1);

//     if (data + sizeof(struct pkt_meta) > data_end) {
//         return 0;
//     }

//     memcpy(pkt_meta, data, sizeof(struct pkt_meta));

//     return 1;
// }

SEC("xdp_redir")
int xdp_redirect(struct xdp_md* ctx) {
    __u32 port = 0;

    int* ifindex = bpf_map_lookup_elem(&xdp_devmap, &port);

    if (!ifindex) {
        return XDP_DROP;
    }

    bpf_printk("xdp_redirect: port=%d\n", port);
    return bpf_redirect(*ifindex, 0);

    // if (bpf_map_lookup_elem(&xdp_devmap, &port)) {
    //     bpf_printk("xdp_redirect: port=%d\n", port);
    //     return bpf_redirect_map(&xdp_devmap, port, 0);
    // }
}
char _license[] SEC("license") = "GPL";
