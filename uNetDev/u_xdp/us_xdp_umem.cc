#include <net/if.h>
#include <bpf/xsk.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <uNetOps.h>
#include <us_xdp_load.hh>

/* Each us_xdp_ring will inject a us_xdp_umem instance as it's base class */
preproc <int bS, int bC, int pS = 4096>
component us_xdp_umem {
    ___api___
    int sk_d;

    us_xdp_umem(){};
    int channel(const char *netdev, int cpu_nr, int xdp_mode);
    int registr(unsigned int umem_flags);
    int rings();

    int getaddr(umem_ring_t ring, int idx);
    ___lib___
    struct sockaddr_xdp     sxdp;

    ___abi___
    struct us_ring f_ring, c_ring, r_ring, t_ring;

    u8 block[bC][bS] __attribute__((aligned(pS)));
};

preproc <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::channel(const char *netdev, int cpu_nr, int xdp_mode) {
    sxdp.sxdp_family = AF_XDP;
    sxdp.sxdp_flags = xdp_mode;
    sxdp.sxdp_ifindex = if_nametoindex(netdev);
    sxdp.sxdp_queue_id = cpu_nr;
    sxdp.sxdp_shared_umem_fd = 0;

    if (0 > (sk_d = socket(AF_XDP, SOCK_RAW, 0))) {
        perror("socket");
        return sk_d;
    }

    return processor(cpu_nr);
}

preproc <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::registr(unsigned int umem_flags) {
    struct xdp_umem_reg umr;
    int rv, blk_count = bC;

    umr.addr = (u64)(uintptr_t)block;
    umr.chunk_size = bS;
    umr.flags = umem_flags;
    umr.headroom = 0;
    umr.len = bS * bC;

    if (0 > (rv = setsockopt(sk_d, SOL_XDP,
                        XDP_UMEM_REG, &umr, sizeof(umr)))) {
                    perror("setsockopt - umem");
                    return rv;
                }

    if (0 > (rv = setsockopt(sk_d, SOL_XDP, 
                        XDP_UMEM_FILL_RING, &blk_count,
                        sizeof(blk_count)))) {
                            perror("setsockopt - f-ring");
                            return rv;
                        }
    if (0 > (rv = setsockopt(sk_d, SOL_XDP, 
                        XDP_UMEM_COMPLETION_RING, &blk_count,
                        sizeof(blk_count)))) {
                            perror("setsockopt - c-ring");
                            return rv;
                        }

    if (0 > (rv = setsockopt(sk_d, SOL_XDP, 
                        XDP_RX_RING, &blk_count,
                        sizeof(blk_count)))) {
                            perror("setsockopt - r-ring");
                            return rv;
                        }

    if (0 > (rv = setsockopt(sk_d, SOL_XDP, 
                        XDP_TX_RING, &blk_count,
                        sizeof(blk_count)))) {
                            perror("setsockopt - t-ring");
                            return rv;
                        }

    return 0;
}

preproc <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::rings() {
    int e;
    void *mm;
    struct xdp_mmap_offsets xmo;
    socklen_t optlen = sizeof(xmo);

    if (0 > (e = getsockopt(sk_d, SOL_XDP,
                        XDP_MMAP_OFFSETS, &xmo,
                        &optlen))) {
                            perror("getsockopt - mmap offsets");
                            return e;
                        }

    if (MAP_FAILED == (mm = mmap(NULL, 
                    xmo.fr.desc + (sizeof(u64) * bC),
                    PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_SHARED,
                    sk_d, XDP_UMEM_PGOFF_FILL_RING))) {
                        perror("mmap -- f-ring");
                        return (int)(uintptr_t)mm;
                    }

    f_ring.ring     = (void *)(xmo.fr.desc + (u8*)mm);
    f_ring.sh_cons  = (u32 *)(xmo.fr.consumer + (u8*)mm);
    f_ring.sh_prod  = (u32 *)(xmo.fr.producer + (u8*)mm);
    f_ring.size     = xmo.fr.desc + (bC * sizeof(u64));
    f_ring.mask     = bC - 1;
    f_ring.flags    = (u32*)(xmo.fr.flags + (u8*)mm);
    f_ring.l_cons   = *(f_ring.sh_cons);
    f_ring.l_prod   = *(f_ring.sh_prod);

    if (MAP_FAILED == (mm = mmap(NULL, 
                    xmo.cr.desc + (sizeof(u64) * bC),
                    PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_SHARED,
                    sk_d, XDP_UMEM_PGOFF_COMPLETION_RING))) {
                        perror("mmap -- c-ring");
                        return (int)(uintptr_t)mm;
                    }

    c_ring.ring     = (void *)(xmo.cr.desc + (u8*)mm);
    c_ring.sh_cons  = (u32 *)(xmo.cr.consumer + (u8*)mm);
    c_ring.sh_prod  = (u32 *)(xmo.cr.producer + (u8*)mm);
    c_ring.size     = xmo.cr.desc + (bC * sizeof(u64));
    c_ring.mask     = bC - 1;
    c_ring.flags    = (u32*)(xmo.cr.flags + (u8*)mm);
    c_ring.l_cons   = *(c_ring.sh_cons);
    c_ring.l_prod   = *(c_ring.sh_prod);

    if (MAP_FAILED == (mm = mmap(NULL, 
                    xmo.rx.desc + (sizeof(struct xdp_desc) * bC),
                    PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_SHARED,
                    sk_d, XDP_PGOFF_RX_RING))) {
                        perror("mmap -- r-ring");
                        return (int)(uintptr_t)mm;
                    }

    r_ring.ring     = (void *)(xmo.rx.desc + (u8*)mm);
    r_ring.sh_cons  = (u32 *)(xmo.rx.consumer + (u8*)mm);
    r_ring.sh_prod  = (u32 *)(xmo.rx.producer + (u8*)mm);
    r_ring.size     = xmo.rx.desc + (bC * sizeof(struct xdp_desc));
    r_ring.mask     = bC - 1;
    r_ring.flags    = (u32*)(xmo.rx.flags + (u8*)mm);
    r_ring.l_cons   = *(r_ring.sh_cons);
    r_ring.l_prod   = *(r_ring.sh_prod);

    if (MAP_FAILED == (mm = mmap(NULL, 
                    xmo.tx.desc + (sizeof(struct xdp_desc) * bC),
                    PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_SHARED,
                    sk_d, XDP_PGOFF_TX_RING))) {
                        perror("mmap -- t-ring");
                        return (int)(uintptr_t)mm;
                    }

    t_ring.ring     = (void *)(xmo.tx.desc + (u8*)mm);
    t_ring.sh_cons  = (u32 *)(xmo.tx.consumer + (u8*)mm);
    t_ring.sh_prod  = (u32 *)(xmo.tx.producer + (u8*)mm);
    t_ring.size     = xmo.tx.desc + (bC * sizeof(struct xdp_desc));
    t_ring.mask     = bC - 1;
    t_ring.flags    = (u32*)(xmo.tx.flags + (u8*)mm);
    t_ring.l_cons   = *(t_ring.sh_cons);
    t_ring.l_prod   = *(t_ring.sh_prod);

    assert(pS == (ptull(f_ring.ring) - ptull(c_ring.ring)));
    assert(pS == (ptull(c_ring.ring) - ptull(r_ring.ring)));
    assert(pS == (ptull(r_ring.ring) - ptull(t_ring.ring)));

    return bind(sk_d, (const sockaddr *)&sxdp, sizeof(sxdp));
}

preproc <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::getaddr(umem_ring_t r, int i) {
    switch (r) {
        case FR:
            goto ____fr__;
        break;
        case CR:
            goto ____cr__;
        break;
        case RR:
        break;
        case TR:
        break;
        default:
            return -EINVAL;
    }
    ____fr__:
    puts("f ring");
    printf("cons %p: %x\n", f_ring.sh_cons, *f_ring.sh_cons);
    printf("prod %p: %x\n", f_ring.sh_prod, *f_ring.sh_prod);
    printf("ring %p: %llx\n", f_ring.ring, *(u64*)f_ring.ring);
    return 0;

    ____cr__:
    puts("c ring");
    printf("cons %p: %x\n", c_ring.sh_cons, *c_ring.sh_cons);
    printf("prod %p: %x\n", c_ring.sh_prod, *c_ring.sh_prod);
    printf("ring %p: %llx\n", c_ring.ring, *(u64*)c_ring.ring);
    return 0;
}

#ifdef TEST
int main (int argc, char *argv[]) {
    struct us_xdp_umem<2048, 2> umem;
    struct us_xdp_load uxl;

    char *netdev    = argv[1];
    char *xskprog   = argv[2];
    int cpu         = atoi(argv[3]);

    uxl.link(xskprog, netdev, XDP_FLAGS_SKB_MODE);

    if (0 > umem.channel(netdev, cpu, XDP_COPY)){
        perror("umem::channel");
        exit(1);
    }

    if (0 > umem.registr(0)) {
        perror("umem::registr");
        exit(1);
    }

    if (0 > umem.rings()) {
        perror("umem::rings");
        exit(1);
    }

    umem.getaddr(FR, 0);
    umem.getaddr(CR, 0);

    uxl.unlink(netdev, XDP_FLAGS_SKB_MODE);
}
#endif