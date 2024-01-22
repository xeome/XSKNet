#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <linux/ip.h>

/**
 * Main XDP program entry point.
 * This is the entry point for all XDP packets. It redirects packets depending on the arbitrary port number defined in eth data
 * payload. It will redirect packets to different virtual interfaces depending on the port number.
 */

#ifndef memcpy
#define memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))
#endif

#define OVER(x, d) (x + 1 > (typeof(x))d)
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

SEC("xdp_redir")
int xdp_redirect(struct xdp_md* ctx) {
    void* data_end = (void*)(long)ctx->data_end;
    void* data = (void*)(long)ctx->data;

    struct ethhdr* eth = data;
    struct iphdr* iph = (struct iphdr*)(eth + 1);

    if (OVER(eth, data_end))
        return XDP_DROP;

    if (eth->h_proto != ntohs(ETH_P_IP))
        return XDP_PASS;

    if (OVER(iph, data_end))
        return XDP_DROP;

    if (iph->protocol != IPPROTO_ICMP)
        return XDP_PASS;

    bpf_printk("Physical interface xdp_redirect------------------");

    bpf_printk("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);

    bpf_printk("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);

    bpf_printk("Source IP: %pI4", &iph->saddr);
    bpf_printk("Dest IP: %pI4", &iph->daddr);

    const __u32 port = 0;

    const int* ifindex = bpf_map_lookup_elem(&xdp_devmap, &port);

    if (!ifindex) {
        return XDP_DROP;
    }

    return bpf_redirect(*ifindex, 0);

    // if (bpf_map_lookup_elem(&xdp_devmap, &port)) {
    //     bpf_printk("xdp_redirect: port=%d\n", port);
    //     return bpf_redirect_map(&xdp_devmap, port, 0);
    // }
}
char _license[] SEC("license") = "GPL";