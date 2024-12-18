#ifndef __cputest_algorithm_h__
#define __cputest_algorithm_h__

#include <stdint.h>

unsigned int prime_count(uint32_t max);
uint64_t fib_value(uint64_t n);
uint64_t xorshift_next(uint64_t x64);
uint64_t xorshift_nstep(uint64_t x64, uint64_t n);
void render_circle(uint32_t *buffer, uint32_t width, uint32_t height, uint32_t cx, uint32_t cy, uint32_t radius);

#endif
