#pragma once
#define pp     template

#include <linux/types.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <linux/if_xdp.h>
#include <linux/if_link.h>
#include <unistd.h>
#ifndef __GNU_SOURCE
#define __GNU_SOURCE
#include <sched.h>
#endif
#include <errno.h>

#ifndef AF_XDP
#define AF_XDP  44
#endif
#ifndef SOL_XDP
#define SOL_XDP 283
#endif
typedef __u64   u64;
typedef __u32   u32;
typedef __u8    u8;

 __attribute__((weak))
int processor(int cpu_nr) {
    cpu_set_t cpu;
    pid_t pid = getpid();
    int rv;

    CPU_ZERO(&cpu);
    CPU_SET(cpu_nr, &cpu);
    if (0 > (rv =sched_setaffinity(pid, sizeof(cpu_set_t), &cpu))) {
        perror("sched_setaffinity");
        return rv;
    }

    return 0;
}

struct us_ring {
    u32 l_prod, l_cons;
    u32 mask, size;
    __volatile__ u32 *sh_prod, *sh_cons;
    __volatile__ void *ring;
    __volatile__ u32 *flags;
};

struct ring_d {
    u64 addr;
    u32 len;
    u32 opts;
};

enum umem_ring_t {
    FR, CR, RR, TR
};

#define ptull(x)    ((u64)(uintptr_t)x)
#include "us_xdp_load.hh"