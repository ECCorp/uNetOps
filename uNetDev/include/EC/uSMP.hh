#ifndef ___U_SMP_HXX___
#define ___U_SMP_HXX___
inline void 
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
u_smp_rb () {
    __asm__ __volatile__("SFENCE":::"memory");
};

inline void 
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
u_smp_wb () {
    __asm__ __volatile__("LFENCE":::"memory");
};

inline void 
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
u_smp_rwb () {
    __asm__ __volatile__("MFENCE":::"memory");
};
    
#endif