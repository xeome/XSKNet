#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>
#include <arpa/inet.h>

#define OVER(x, d) (x + 1 > (typeof(x))d)

SEC("xdp_redirect_dummy")
int xdp_redirect_dummy_prog(struct xdp_md* ctx) {
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

    bpf_printk("Dummy interface------------------");

    bpf_printk("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);

    bpf_printk("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);

    bpf_printk("Source IP: %pI4", &iph->saddr);
    bpf_printk("Dest IP: %pI4\n", &iph->daddr);

    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";