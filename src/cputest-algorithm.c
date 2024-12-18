#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "cputest-algorithm.h"

// 将这些算法实现放在一个单独的文件中
// 避免由于调用处被编译器能看到实现细节而造成激进的常量折叠优化

static inline bool prime_check(uint32_t n)
{
    if (n < 2)
    {
        return false;
    }
    uint32_t test_upper_bound = (uint32_t)sqrt(n);
    for (uint32_t i = 2; i <= test_upper_bound; i++)
    {
        if (n % i == 0)
        {
            return false;
        }
    }
    return true;
}

unsigned int prime_count(uint32_t max)
{
    unsigned int count = 0;
    for (uint32_t i = 1; i <= max; i++)
    {
        if (prime_check(i))
        {
            count++;
        }
    }
    return count;
}

uint64_t fib_value(uint64_t n)
{
    uint64_t a = 0;
    uint64_t b = 1;

    for (uint64_t i = 0; i < n; i++)
    {
        uint64_t t = a + b;
        a = b;
        b = t;
    }
    return a;
}

uint64_t xorshift_next(uint64_t x64)
{
	x64 ^= x64 << 13;
	x64 ^= x64 >> 7;
	x64 ^= x64 << 17;
	return x64;
}

uint64_t xorshift_nstep(uint64_t x64, uint64_t n)
{
    for (unsigned int i = 0; i < n; i++)
    {
        x64 = xorshift_next(x64);
    }
    return x64;
}

void render_circle(uint32_t *buffer, uint32_t width, uint32_t height, uint32_t cx, uint32_t cy, uint32_t radius)
{
    uint32_t left_top_color = 0x00ff0000;
    uint32_t right_top_color = 0x0000ff00u;
    uint32_t left_bottom_color = 0x000000ffu;
    uint32_t right_bottom_color = 0x00ffffff;

    memset(buffer, 0, width * height * sizeof(uint32_t));

    uint32_t left, right, top, bottom;
    if (cx < radius)
    {
        left = 0;
    }
    else
    {
        left = cx - radius;
    }
    if (cx + radius >= width)
    {
        right = width - 1;
    }
    else
    {
        right = cx + radius;
    }
    if (cy < radius)
    {
        top = 0;
    }
    else
    {
        top = cy - radius;
    }
    if (cy + radius >= height)
    {
        bottom = height - 1;
    }
    else
    {
        bottom = cy + radius;
    }

    // extern int printf(const char *format, ...);
    // printf("loop times: %u\n", (right - left + 1) * (bottom - top + 1));
    for (unsigned int y = top; y <= bottom; y++)
    {
        for (unsigned int x = left; x <= right; x++)
        {
            int32_t dx = (int32_t)(x - cx);
            int32_t dy = (int32_t)(y - cy);
            uint32_t distance = (uint32_t)sqrt(dx * dx + dy * dy);

            float normalized_distance = (float)distance / radius;
            float sin_value = fabs(sinf(normalized_distance * 10));

            if (distance <= radius)
            {
                uint32_t color = 0;
                if (dx < 0 && dy < 0)
                {
                    color = left_top_color;
                }
                else if (dx >= 0 && dy < 0)
                {
                    color = right_top_color;
                }
                else if (dx < 0 && dy >= 0)
                {
                    color = left_bottom_color;
                }
                else
                {
                    color = right_bottom_color;
                }


                uint8_t *color_ptr = (uint8_t *)&color; // maybe canbe vectorized, but I do not care
                float m = normalized_distance * sin_value;
                color_ptr[0] = (uint8_t)(color_ptr[0] * m);
                color_ptr[1] = (uint8_t)(color_ptr[1] * m);
                color_ptr[2] = (uint8_t)(color_ptr[2] * m);
                color_ptr[3] = (uint8_t)(color_ptr[3] * m);

                buffer[y * width + x] = color;
            }
        }
    }
}
