#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#ifndef NUMCPUS
#define NUMCPUS 64
#endif

#define bpf_custom_printk(fmt, ...)                     \
        ({                                              \
            char ____fmt[] = fmt;                       \
            bpf_trace_printk(____fmt, sizeof(____fmt),  \
                    ##__VA_ARGS__);                     \
        })

struct bpf_map_def SEC("maps") xsks_map = {
    .type = BPF_MAP_TYPE_XSKMAP,
    .key_size = sizeof(int),
    .value_size = sizeof(int),
    .max_entries = NUMCPUS
};

struct bpf_map_def SEC("maps") xsk_cpus = {
    .type = BPF_MAP_TYPE_PERCPU_ARRAY,
    .key_size = sizeof(int),
    .value_size = sizeof(__u64),
    .max_entries = NUMCPUS
};

int SEC("xdp_sock") xsk(struct xdp_md *ctx) {
    int qid = ctx->rx_queue_index;
    __u64 *per_cpu_count;

    per_cpu_count = bpf_map_lookup_elem(&xsk_cpus, &qid);
    if (per_cpu_count)
        (*per_cpu_count)++;

    if (bpf_map_lookup_elem(&xsks_map, &qid)) {
        return bpf_redirect_map(&xsks_map, qid, 0);
    }

    return XDP_PASS;
}
char _license[] SEC("license") = "GPL";