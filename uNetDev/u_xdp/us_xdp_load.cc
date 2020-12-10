#include <linux/if_link.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>

#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uNetDev/uNetDev.hh>

#define __lbpf__pe(class, lbpf_e, user_str)        do { libbpf_strerror(lbpf_e, __lbpferr__, sizeof(__lbpferr__)); \
                                    fprintf(stderr, "%s::%s: %s @ %d => %s\n", #class, __func__, __lbpferr__, __LINE__, user_str); } while (0)

int us_xdp_load::link(const char *file, const char *dev, int xdp_mode) {
    int bpf_error;
    struct bpf_prog_load_attr bpf_prog_attr;

    memset(&bpf_prog_attr, 0, sizeof(bpf_prog_attr));

    bpf_prog_attr.file = file;
    bpf_prog_attr.prog_type = BPF_PROG_TYPE_XDP;

    if (0 > (bpf_error = bpf_prog_load_xattr(&bpf_prog_attr, 
                                                &obj, &progdesc))) {
        __lbpf__pe(us_xdp_load, bpf_error, "exiting");
        exit(1);
    }

    if (progdesc < 0) {
        fprintf(stderr, "No program found => exiting\n");
        exit(1);
    }

    if (0 > (bpf_error = bpf_set_link_xdp_fd(if_nametoindex(dev), 
                                        progdesc, xdp_mode))) {
        __lbpf__pe(us_xdp_load, bpf_error, "returning bpf errorcode");
        return bpf_error;
    }
    return 0;
}

int us_xdp_load::unlink(const char *dev, int xdp_mode) {
    int bpf_error;
    if (0 > (bpf_error = bpf_set_link_xdp_fd(if_nametoindex(dev), 
                                        -1, xdp_mode))) {
        __lbpf__pe(us_xdp_load, bpf_error, "returning bpf errorcode");
        return bpf_error;
    }

    return 0;
}

int us_xdp_load::getelem(const char *mapname, void * key, void * value) {
    int map_fd, lbpf_err;

    if (0 > (map_fd = bpf_object__find_map_fd_by_name(obj, mapname))) {
        __lbpf__pe(us_xdp_load, map_fd, "returning bpf errorcode");
        return map_fd;
    }

    if (0 > (lbpf_err = bpf_map_lookup_elem(map_fd, key, value))) {
        __lbpf__pe(us_xdp_load, lbpf_err, "returning bpf errorcode");
        return lbpf_err;
    }

    return 0;
}

int us_xdp_load::setelem(const char *mapname, void *key, void *value) {
    int map_fd, lbpf_err;

    if (0 > (map_fd = bpf_object__find_map_fd_by_name(obj, mapname))) {
        __lbpf__pe(us_xdp_load, map_fd, "returning bpf errorcode");
        return map_fd;
    }

    if (0 > (lbpf_err = bpf_map_update_elem(map_fd, key, value, BPF_ANY))) {
        __lbpf__pe(us_xdp_load, lbpf_err, "returning bpf errorcode");
        return lbpf_err;
    }

    return 0;
}

#ifdef TEST
int main (int argc, char *argv[]) {
	struct us_xdp_load xf;
    int key, rv, cpu_nr, cpu_nr2;
    unsigned long long svalue, gvalue, oldv;

    xf.link(argv[1], argv[2], XDP_FLAGS_SKB_MODE);

    rv = xf.getelem("xsk_cpus", &key, &gvalue);
    if (0 > rv)
        exit(1);
    printf("Element Initially => %llx\n", gvalue);

    svalue = 63;
    rv = xf.setelem("xsk_cpus", &key, &svalue);
    if (0 > rv)
        exit(1);

    rv = xf.getelem("xsk_cpus", &key, &gvalue);
    if (0 > rv)
        exit(1);
    printf("Element Set To => %llx\n", gvalue);

    oldv = svalue;
    while (oldv == gvalue) {
        cpu_nr = sched_getcpu();
        xf.getelem("xsk_cpus", &key, &gvalue);
        cpu_nr2 = sched_getcpu();
    }
    printf("%llx on CPU%d or CPU%d\n", gvalue, cpu_nr, cpu_nr2);
    xf.unlink(argv[2], XDP_FLAGS_SKB_MODE);
    
    return 0;
}
#endif
