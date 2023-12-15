/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>
#include <arpa/inet.h>

#define OVER(x, d) (x + 1 > (typeof(x))d)

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __type(key, __u32);
    __type(value, __u32);
    __uint(max_entries, 64);
} xsks_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __type(key, __u32);
    __type(value, __u32);
    __uint(max_entries, 64);
} xdp_stats_map SEC(".maps");

SEC("xdp")
int xdp_sock_prog(struct xdp_md* ctx) {
    int index = ctx->rx_queue_index;

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
    bpf_printk("AF_XDP socket program-------------");

    bpf_printk("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_source[0], eth->h_source[1], eth->h_source[2],
               eth->h_source[3], eth->h_source[4], eth->h_source[5]);

    bpf_printk("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3],
               eth->h_dest[4], eth->h_dest[5]);

    bpf_printk("Source IP: %pI4", &iph->saddr);
    bpf_printk("Dest IP: %pI4", &iph->daddr);

    /* A set entry here means that the correspnding queue_id
     * has an active AF_XDP socket bound to it. */
    if (bpf_map_lookup_elem(&xsks_map, &index))
        return bpf_redirect_map(&xsks_map, index, 0);

    return XDP_DROP;
}

char _license[] SEC("license") = "GPL";
