#include <stdio.h>
#include <stdlib.h>
#include "cputest-mat.h"
#include "multitask.h"

Mat* mat_new()
{
    Mat e = {};
    Mat *mat = (Mat *)mt_alloc(sizeof(Mat));
    *mat = e;
    return mat;
}

void mat_delete(Mat *mat)
{
    if (mat->data)
    {
        mt_free(mat->data, mat->data_len);
    }
    mt_free(mat, sizeof(Mat));
}

void mat_set_shape(Mat *mat, unsigned int row, unsigned int col)
{
    if (row == mat->row && col == mat->col)
    {
        return;
    }
    size_t size_want = row * col * sizeof(float);
    mat->row = row;
    mat->col = col;
    if (size_want <= mat->data_len)
    {
        return;
    }
    if (mat->data)
    {
        mt_free(mat->data, mat->data_len);
    }
    mat->data_len = size_want;
    mat->data = mt_alloc(mat->data_len);
}

void mat_mul(const Mat *a, const Mat *b, Mat *c)
{
    if (a->col != b->row)
    {
        fprintf(stderr, "Matrix size mismatch: (%ux%u)x(%ux%u)\n", a->row, a->col, b->row, b->col);
        abort();
    }

    mat_set_shape(c, a->row, b->col);
    for (unsigned int i = 0; i < a->row; i++)
    {
        for (unsigned int j = 0; j < b->col; j++)
        {
            c->data[i * c->col + j] = 0.0;
            for (unsigned int k = 0; k < a->col; k++)
            {
                c->data[i * c->col + j] += a->data[i * a->col + k] * b->data[k * b->col + j];
            }
        }
    }
}

void mat_add(const Mat *a, const Mat *b, Mat *c)
{
    if (a->row != b->row || a->col != b->col)
    {
        fprintf(stderr, "Matrix size mismatch: (%ux%u)+(%ux%u)\n", a->row, a->col, b->row, b->col);
        abort();
    }

    mat_set_shape(c, a->row, a->col);
    for (unsigned int i = 0; i < a->row; i++)
    {
        for (unsigned int j = 0; j < a->col; j++)
        {
            c->data[i * c->col + j] = a->data[i * a->col + j] + b->data[i * b->col + j];
        }
    }
}

void mat_conv(const Mat *src, const Mat *kernel, Mat *output)
{
    if (src->row < kernel->row || src->col < kernel->col)
    {
        fprintf(stderr, "Source matrix size must be larger than kernel size\n");
        abort();
    }
    if (kernel->row % 2 == 0 || kernel->col % 2 == 0)
    {
        fprintf(stderr, "Kernel size must be odd\n");
        abort();
    }

    mat_set_shape(output, src->row, src->col);
    for (int i = 0; i < (int)src->row; i++)
    {
        for (int j = 0; j < (int)src->col; j++)
        {
            float out_tmp = 0.0f;
            int x_start = i - kernel->row / 2;
            int y_start = j - kernel->col / 2;

            for (int ker_i = 0; ker_i < (int)kernel->row; ker_i++)
            {
                for (int ker_j = 0; ker_j < (int)kernel->col; ker_j++)
                {
                    int src_x = x_start + ker_i;
                    int src_y = y_start + ker_j;
                    if (src_x < 0 || src_x >= (int)src->row || src_y < 0 || src_y >= (int)src->col)
                    {
                        continue;
                    }
                    out_tmp += src->data[src_x * src->col + src_y] * kernel->data[ker_i * kernel->col + ker_j];
                }
            }
            output->data[i * output->col + j] = out_tmp;
        }
    }
}

void mat_dump(FILE *f, const Mat *mat)
{
    for (unsigned int i = 0; i < mat->row; i++)
    {
        for (unsigned int j = 0; j < mat->col; j++)
        {
            fprintf(f, "%-9.2f ", mat->data[i * mat->col + j]);
        }
        fprintf(f, "\n");
    }
}

bool mat_almost_equal(const Mat *a, const Mat *b)
{
    if (a->row != b->row || a->col != b->col)
    {
        return false;
    }
    for (unsigned int i = 0; i < a->row; i++)
    {
        for (unsigned int j = 0; j < a->col; j++)
        {
            float diff = a->data[i * a->col + j] - b->data[i * b->col + j];
            if (!(-1e-6 < diff && diff < 1e-6))
            {
                return false;
            }
        }
    }
    return true;
}
