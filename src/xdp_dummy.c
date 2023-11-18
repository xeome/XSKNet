#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("xdp_redirect_dummy")
int xdp_redirect_dummy_prog(struct xdp_md* ctx) {
    return XDP_PASS;
}
