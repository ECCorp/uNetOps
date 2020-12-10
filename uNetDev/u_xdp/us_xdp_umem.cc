#include <net/if.h>
#include <bpf/xsk.h>
#include <sys/mman.h>
#include <x86intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <EC/uSMP.h>
#include <uNetDev/uNetDev.hh>
#include <poll.h>
#include <sys/resource.h>

/* one us_xdp_umem is shared for each cpu */
pp <int bS, int bC, int pS = 4096>
struct us_xdp_umem {
    int sk_d, xsks;
    struct sockaddr_xdp     sxdp;
    __volatile__ u8 *block;
    struct us_ring f_ring, c_ring, r_ring, t_ring;

    us_xdp_umem(){};
    int channel(const char *netdev, int cpu_nr, int xdp_mode);
    int registr(unsigned int umem_flags);
    int rings();
    int fill(u64 umem_adr);
    struct ring_d &rx();
    void comp();
    int tx();
};

const static inline u32 ldiff_prcr(struct us_ring *r) {
    return r->l_prod - r->l_cons;
}

const static inline u32 ldiff_crpr(struct us_ring *r) {
    return r->l_cons - r->l_prod;
}

const static inline int fr_count(struct us_ring *r) {
    u32 nfr_blocks;

    if (0 == (nfr_blocks = ldiff_prcr(r))) {
        r->l_prod = *r->sh_prod;
        return ldiff_prcr(r);
    }

    return nfr_blocks;
}

pp <int bS, int bC, int pS>
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

pp <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::registr(unsigned int umem_flags) {
    struct xdp_umem_reg umr;
    int rv, blk_count = bC;
    
    block = (volatile u8*)aligned_alloc(pS, bS * bC);

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

pp <int bS, int bC, int pS>
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

    r_ring.ring     = (void *)(xmo.rx.desc + ((u8*)mm));
    r_ring.sh_cons  = (u32 *)(xmo.rx.consumer + ((u8*)mm));
    r_ring.sh_prod  = (u32 *)(xmo.rx.producer + ((u8*)mm));
    r_ring.size     = xmo.rx.desc + (bC * sizeof(struct xdp_desc));
    r_ring.mask     = bC - 1;
    r_ring.flags    = (u32*)(xmo.rx.flags + ((u8*)mm));
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

    t_ring.ring     = (void *)(xmo.tx.desc + ((u8*)mm));
    t_ring.sh_cons  = (u32 *)(xmo.tx.consumer + ((u8*)mm));
    t_ring.sh_prod  = (u32 *)(xmo.tx.producer + ((u8*)mm));
    t_ring.size     = xmo.tx.desc + (bC * sizeof(struct xdp_desc));
    t_ring.mask     = bC - 1;
    t_ring.flags    = (u32*)(xmo.tx.flags + (u8*)mm);
    t_ring.l_cons   = *(t_ring.sh_cons);
    t_ring.l_prod   = *(t_ring.sh_prod);

    //assert(pS == (ptull(f_ring.ring) - ptull(c_ring.ring)));
    //assert(pS == (ptull(c_ring.ring) - ptull(r_ring.ring)));
    //assert(pS == (ptull(r_ring.ring) - ptull(t_ring.ring)));

    if (0 > (e = bind(sk_d, (const sockaddr *)&sxdp, 
                            sizeof(sxdp)))) {
                                perror("bind");
                                return e;
                            }
    xsks = 1;

    return 0;
}

pp <int bS, int bC, int pS>
int us_xdp_umem<bS, bC, pS>::fill(u64 umem_adr){
    u32 nfr_blocks;

    nfr_blocks = fr_count(&f_ring);
    ((u64*)f_ring.ring)[f_ring.l_prod++ & (f_ring.mask - 1)] = umem_adr;

    asm("":::"memory");
    (*f_ring.sh_prod)++;

    return 1 + nfr_blocks;
}

#define TEST
#ifdef TEST
int main (int argc, char *argv[]) {
    struct us_xdp_umem<2048, 2> umem;
    struct us_xdp_load uxl;
    struct rlimit rl = {(rlim_t)-1, (rlim_t)-1};

    if (0 > setrlimit(RLIMIT_MEMLOCK, &rl)) {
        perror("setrlimit -- RLIMIT_MEMLOCK");
        exit(1);
    }

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

    umem.xsks = 1;
    uxl.setelem("xsks_map", &umem.xsks, &umem.sk_d);
    u32 gv = 0, olv = 0;
    umem.fill(umem.block);
puts("a");
    struct pollfd pfd;
    pfd.fd = umem.sk_d;
    pfd.events = POLLIN;
    poll(&pfd, 1, 1);
int x = 0;
    u32 prev_cons = *umem.f_ring.sh_cons;

    while (prev_cons == *umem.f_ring.sh_cons) {
if (x++ == 0)
puts("b"); 
        uxl.getelem("xsk_cpus", &cpu, &gv);
        if (gv != olv) {
            printf("fr %p c %d p %d r p %d\n", umem.f_ring.sh_prod, *umem.f_ring.sh_cons, *umem.f_ring.sh_prod, *umem.r_ring.sh_prod);
            olv = gv;
        }
    }
    printf("PACKET IN RING --> producer %u => %u\n", prev_cons, *umem.f_ring.sh_cons);

    uxl.unlink(netdev, XDP_FLAGS_SKB_MODE);

    return 0;
}
#endif