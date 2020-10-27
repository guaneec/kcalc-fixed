#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

extern const uint64_t INF_INT;
extern const uint64_t NAN_INT;

typedef union __fixedp {
    struct {
        uint32_t frac;
        uint32_t inte;
    };
    uint64_t data;
} fixedp;

#endif /* FIXED_POINT_H */
