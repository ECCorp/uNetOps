#pragma once
#include <xtd.hh>
__macro_fn(void) 
u_smp_rb () {
    __as("SFENCE":::"memory");
};

__macro_fn(void)
u_smp_wb () {
    __as("LFENCE":::"memory");
};

__macro_fn(void) 
u_smp_rwb () {
    __as("MFENCE":::"memory");
};

#ifndef __GNU_SOURCE
#define __GNU_SOURCE
#include <sched.h>
#endif
#include <unistd.h>

__macro_fn(int) 
u_smp_set_cpu(int cpu_nr) {
		int e;
    cpu_set_t cs;
    CPU_ZERO( &cs );
    CPU_SET( cpu_nr , &cs );
    if ( 0 > ( e = sched_setaffinity( getpid() , sizeof(cs) , &cs ))) {
#ifndef __PROD__
        perror("u_smp_set_cpu");
#endif
        return e;
    }
    return 0;
}
