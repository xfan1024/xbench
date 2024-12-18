#ifndef __cputest_mat_h__
#define __cputest_mat_h__

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    float *data;
    size_t data_len;
    unsigned int row;
    unsigned int col;
} Mat;

Mat* mat_new();
void mat_delete(Mat *mat);
void mat_set_shape(Mat *mat, unsigned int row, unsigned int col);
void mat_mul(const Mat *a, const Mat *b, Mat *c);
void mat_add(const Mat *a, const Mat *b, Mat *c);
void mat_conv(const Mat *src, const Mat *kernel, Mat *output);
bool mat_almost_equal(const Mat *a, const Mat *b);
void mat_dump(FILE *f, const Mat *mat);

#endif
