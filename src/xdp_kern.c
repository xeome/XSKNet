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

static __always_inline int parse_ethhdr(struct ethhdr* eth, void* data_end, struct pkt_meta* pkt_meta) {
    void* data = (void*)(eth + 1);

    if (data + sizeof(struct pkt_meta) > data_end) {
        return 0;
    }

    memcpy(pkt_meta, data, sizeof(struct pkt_meta));

    return 1;
}

SEC("xdp_redir")
int xdp_redirect(struct xdp_md* ctx) {
    // For now redirect packets to veth1 and veth2, round robin style
    __u32 *pkt_count, key = 0;
    pkt_count = bpf_map_lookup_elem(&xdp_stats_map, &key);
    __u16 base_ifindex = 10;  // Last regular interface I have on my machine, rest will be virtual

    if (pkt_count) {
        if ((*pkt_count)++ & 1) {
            bpf_printk("redirecting to veth1\n");
            return bpf_redirect(base_ifindex + 1, 0);
        } else {
            bpf_printk("redirecting to veth2\n");
            return bpf_redirect(base_ifindex + 2, 0);
        }
    } else {
        bpf_printk("pkt_count is null\n");
        return XDP_DROP;
    }

    return XDP_DROP;
}
char _license[] SEC("license") = "GPL";
