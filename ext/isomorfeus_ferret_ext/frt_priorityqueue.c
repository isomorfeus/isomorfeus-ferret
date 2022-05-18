#include <string.h>
#include "frt_priorityqueue.h"

FrtPriorityQueue *frt_pq_new(unsigned int type_size, int capa, frt_lt_ft less_than, frt_free_ft free_elem) {
    FrtPriorityQueue *pq = FRT_ALLOC(FrtPriorityQueue);
    pq->type_size = (type_size > sizeof(void *)) ? (type_size / sizeof(void *)) : 1;
    pq->void_size = pq->type_size * sizeof(void *);
    pq->size = 0;
    pq->capa = capa;
    pq->mem_capa = (FRT_PQ_START_CAPA > capa ? capa : FRT_PQ_START_CAPA);
    pq->heap = frt_ecalloc(pq->void_size * pq->mem_capa);
    pq->less_than_i = less_than;
    pq->proc = Qnil;

    /* need to set this yourself if you want to change it */
    pq->free_elem_i = free_elem ? free_elem : &frt_dummy_free;
    return pq;
}

FrtPriorityQueue *frt_pq_clone(FrtPriorityQueue *pq) {
    FrtPriorityQueue *new_pq = FRT_ALLOC(FrtPriorityQueue);
    memcpy(new_pq, pq, sizeof(FrtPriorityQueue));
    new_pq->heap = frt_ecalloc(pq->void_size * new_pq->mem_capa);
    memcpy(new_pq->heap, pq->heap, pq->void_size * new_pq->size);
    return new_pq;
}

void frt_pq_clear(FrtPriorityQueue *pq) {
    int i;
    for (i = 1; i <= pq->size; i++) {
        pq->free_elem_i(pq->heap[pq->type_size * i]);
        pq->heap[pq->type_size * i] = NULL;
    }
    pq->size = 0;
}

void frt_pq_free(FrtPriorityQueue *pq) {
    free(pq->heap);
    free(pq);
}

void frt_pq_destroy(FrtPriorityQueue *pq) {
    frt_pq_clear(pq);
    frt_pq_free(pq);
}

/**
 * This method is used internally by frt_pq_push. It is similar to frt_pq_down except
 * that where frt_pq_down reorders the elements from the top, pq_up reorders from
 * the bottom.
 *
 * @param pq the PriorityQueue to reorder
 */
static void frt_pq_up(FrtPriorityQueue *pq) {
    void **heap = pq->heap;
    void *node;
    int i = pq->size;
    int j = i >> 1;

    node = heap[pq->type_size * i];

    while ((j > 0) && pq->less_than_i(node, heap[pq->type_size * j], pq->proc)) {
        heap[pq->type_size * i] = heap[pq->type_size * j];
        i = j;
        j = j >> 1;
    }
    heap[pq->type_size * i] = node;
}

void frt_pq_down(FrtPriorityQueue *pq) {
    register int i = 1;
    register int j = 2;         /* i << 1; */
    register int k = 3;         /* j + 1;  */
    register int size = pq->size;
    void **heap = pq->heap;
    void *node = heap[pq->type_size * i];       /* save top node */

    if ((k <= size) && (pq->less_than_i(heap[pq->type_size * k], heap[pq->type_size * j], pq->proc))) {
        j = k;
    }

    while ((j <= size) && pq->less_than_i(heap[pq->type_size * j], node, pq->proc)) {
        heap[pq->type_size * i] = heap[pq->type_size * j];      /* shift up child */
        i = j;
        j = i << 1;
        k = j + 1;
        if ((k <= size) && pq->less_than_i(heap[pq->type_size * k], heap[pq->type_size * j], pq->proc)) {
            j = k;
        }
    }
    heap[pq->type_size * i] = node;
}

void frt_pq_push(FrtPriorityQueue *pq, void *elem) {
    pq->size++;
    if (pq->size >= pq->mem_capa) {
        int old_mem_capa = pq->mem_capa;
        int i;
        pq->mem_capa <<= 1;
        pq->heap = frt_erealloc(pq->heap, pq->void_size * pq->mem_capa);
        memset(pq->heap + (pq->void_size * old_mem_capa), 0, (pq->void_size * pq->mem_capa) - (pq->void_size * old_mem_capa));
    }
    pq->heap[pq->type_size * pq->size] = elem;
    frt_pq_up(pq);
}

FrtPriorityQueueInsertEnum frt_pq_insert(FrtPriorityQueue *pq, void *elem) {
    if (pq->size < pq->capa) {
        frt_pq_push(pq, elem);
        return FRT_PQ_ADDED;
    }

    if (pq->size > 0 && pq->less_than_i(pq->heap[pq->type_size * 1], elem, pq->proc)) {
        pq->free_elem_i(pq->heap[pq->type_size * 1]);
        pq->heap[pq->type_size * 1] = elem;
        frt_pq_down(pq);
        return FRT_PQ_INSERTED;
    }

    pq->free_elem_i(elem);
    return FRT_PQ_DROPPED;
}

void *frt_pq_top(FrtPriorityQueue *pq) {
    return pq->size ? pq->heap[pq->type_size * 1] : NULL;
}

void *frt_pq_pop(FrtPriorityQueue *pq) {
    if (pq->size > 0) {
        void *result = pq->heap[pq->type_size * 1];       /* save first value */
        pq->heap[pq->type_size * 1] = pq->heap[pq->type_size * pq->size]; /* move last to first */
        pq->heap[pq->type_size * pq->size] = NULL;
        pq->size--;
        frt_pq_down(pq);                      /* adjust heap */
        return result;
    }
    return NULL;
}
