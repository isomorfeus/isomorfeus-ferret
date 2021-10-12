#include "ruby.h"
#include "frt_array.h"
#include <string.h>

#define META_CNT FRT_ARY_META_CNT
#define DATA_SZ sizeof(int) * META_CNT

void **frt_ary_new_i(int type_size, int init_capa)
{
    void **ary;
    if (init_capa <= 0) {
        init_capa = FRT_ARY_INIT_CAPA;
    }
    ary = (void **)&(((int *)frt_ecalloc(DATA_SZ +
                                        init_capa * type_size))[META_CNT]);
    frt_ary_type_size(ary) = type_size;
    frt_ary_capa(ary) = init_capa;
    return ary;
}

void frt_ary_resize_i(void ***ary, int size)
{
    size++;
    if (size > frt_ary_sz(*ary)) {
        int capa = frt_ary_capa(*ary);
        if (size >= capa) {
            int *frt_ary_start = &((int *)*ary)[-META_CNT];
            while (size >= capa) {
                capa <<= 1;
            }

            frt_ary_start = (int *)frt_erealloc(frt_ary_start,
                                        DATA_SZ + capa * frt_ary_type_size(*ary));
            *ary = (void **)&(frt_ary_start[META_CNT]);
            memset(((char *)*ary) + frt_ary_type_size(*ary) * frt_ary_sz(*ary), 0,
                   (capa - frt_ary_sz(*ary)) * frt_ary_type_size(*ary));
            frt_ary_capa(*ary) = capa;
        }
        frt_ary_sz(*ary) = size;
    }
}

void frt_ary_push_i(void ***ary, void *value)
{
    int size = frt_ary_sz(*ary);
    frt_ary_resize_i(ary, size);
    (*ary)[size] = value;
}

void frt_ary_destroy_i(void **ary, void (*free_elem)(void *p))
{
    int i;
    for (i = frt_ary_sz(ary) - 1; i >= 0; i--) {
        free_elem(ary[i]);
    }
    frt_ary_free(ary);
}
