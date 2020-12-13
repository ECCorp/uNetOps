#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <net/if.h>
#include <sys/resource.h>
#include <bpf/xsk.h>
#include <poll.h>
#include <unistd.h>

#include <u_smp.hh>
#include <u_xdp.hh>

namespace us_xdp {
    template <
        u32 BlockSize,
        u32 BlockCount
    >
    struct umem {
        umem();
    };
};
us_xdp::umem<u32 BS, u32 BC>::umem() {
    
}

#define TEST
#ifdef TEST
int main (int argc, char * argv[]) {
    int n_blocks;
    __u32 fr_idx, rx_idx;
    struct pollfd pfd;
    struct us_xdp_load uxl;
    struct xsk_socket_config xscnf;
    struct xsk_umem_config umcnf;
    struct xsk_umem *umem;
    struct xsk_socket *xsk;
    struct xsk_ring_prod f_ring;
    struct xsk_ring_cons c_ring;
    struct xsk_ring_prod tx;
    struct xsk_ring_cons rx;
    void *mm = nullptr;
    char * devname  = argv[1];
    int blksz       = atoi(argv[3]);
    int blkcnt      = atoi(argv[4]);
    int cpu         = atoi(argv[5]);
    processor(cpu);

    if (MAP_FAILED == (mm = mmap(NULL,
                            blksz * blkcnt,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS,
                            -1, 0))) {
                                perror("mmap");
                                exit(1);
                            }

    umcnf.comp_size = blkcnt;
    umcnf.fill_size = blkcnt;
    umcnf.flags = 0;
    umcnf.frame_headroom = 0;
    umcnf.frame_size = blksz;

    umem = (struct xsk_umem *)calloc(1, sizeof(umem));
    if (0 > xsk_umem__create(&umem, mm, 
                blksz * blkcnt, &f_ring, &c_ring,
                &umcnf)) {
                    perror("xsk_umem__create");
                    exit(1);
                }

    xscnf.bind_flags = XDP_COPY;
    xscnf.libbpf_flags = 0;
    xscnf.rx_size = blkcnt;
    xscnf.tx_size = blkcnt;
    xscnf.xdp_flags = XDP_FLAGS_SKB_MODE;
    if (0 > xsk_socket__create(&xsk, devname, 0,
                        umem, &rx, &tx, &xscnf)) {
                            perror("xsk_socket__create");
                            exit(1);
                        }

    if (0 > (n_blocks = xsk_ring_prod__reserve(&f_ring, blkcnt, (le32*)&fr_idx))) {
        perror("xsk_ring_prod__reserve");
        exit(1);
    }

    for (int i = 0; i < n_blocks; i++) {
        *(xsk_ring_prod__fill_addr(&f_ring, fr_idx++)) = i * blksz;
    }

    xsk_ring_prod__submit(&f_ring, blkcnt);

    pfd.fd = xsk_socket__fd(xsk);
    pfd.events = POLLIN;
    poll(&pfd, 1, -1);

    const struct xdp_desc *xd;
    size_t nb0, nb1;tcj
    
    do {
        poll(&pfd, 1, -1);
        nb0 = xsk_ring_cons__peek(&rx, blkcnt, &rx_idx); 
        xd = xsk_ring_cons__rx_desc(&rx, rx_idx++);


        xsk_ring_cons__release(&rx, nb0);

        nb1 = xsk_ring_prod__reserve(&f_ring, blkcnt, &fr_idx);

        for (int i = 0; i < (int)nb1; i++) {
            *(xsk_ring_prod__fill_addr(&f_ring, fr_idx++)) = i * blksz;
        }
        xsk_ring_prod__submit(&f_ring, nb1);
    } while (true);

}
#endif