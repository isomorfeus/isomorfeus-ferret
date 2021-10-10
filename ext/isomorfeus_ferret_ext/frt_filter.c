#include "frt_search.h"
#include "frt_symbol.h"
#include <string.h>
#include "frt_internal.h"

/***************************************************************************
 *
 * Filter
 *
 ***************************************************************************/

void filt_destroy_i(FrtFilter *filt)
{
    h_destroy(filt->cache);
    free(filt);
}

void filt_deref(FrtFilter *filt)
{
    if (--(filt->ref_cnt) == 0) {
        filt->destroy_i(filt);
    }
}

FrtBitVector *filt_get_bv(FrtFilter *filt, IndexReader *ir)
{
    FrtCacheObject *co = (FrtCacheObject *)h_get(filt->cache, ir);

    if (!co) {
        FrtBitVector *bv;
        if (!ir->cache) {
            ir_add_cache(ir);
        }
        bv = filt->get_bv_i(filt, ir);
        co = co_create(filt->cache, ir->cache, filt, ir,
                       (free_ft)&bv_destroy, (void *)bv);
    }
    return (FrtBitVector *)co->obj;
}

static char *filt_to_s_i(FrtFilter *filt)
{
    return estrdup(filt->name);
}

static unsigned long long filt_hash_default(FrtFilter *filt)
{
    (void)filt;
    return 0;
}

static int filt_eq_default(FrtFilter *filt, FrtFilter *o)
{
    (void)filt; (void)o;
    return false;
}

FrtFilter *filt_create(size_t size, FrtSymbol name)
{
    FrtFilter *filt    = (FrtFilter *)emalloc(size);
    filt->cache     = co_hash_create();
    filt->name      = name;
    filt->to_s      = &filt_to_s_i;
    filt->hash      = &filt_hash_default;
    filt->eq        = &filt_eq_default;
    filt->destroy_i = &filt_destroy_i;
    filt->ref_cnt   = 1;
    return filt;
}

unsigned long long filt_hash(FrtFilter *filt)
{
    return sym_hash(filt->name) ^ filt->hash(filt);
}

int filt_eq(FrtFilter *filt, FrtFilter *o)
{
    return ((filt == o)
            || ((strcmp(filt->name, o->name) == 0)
                && (filt->eq == o->eq)
                && (filt->eq(filt, o))));
}

/***************************************************************************
 *
 * QueryFilter
 *
 ***************************************************************************/

#define QF(filt) ((QueryFilter *)(filt))
typedef struct QueryFilter
{
    FrtFilter super;
    Query *query;
} QueryFilter;

static char *qfilt_to_s(FrtFilter *filt)
{
    Query *query = QF(filt)->query;
    char *query_str = query->to_s(query, NULL);
    char *filter_str = strfmt("QueryFilter< %s >", query_str);
    free(query_str);
    return filter_str;
}

static FrtBitVector *qfilt_get_bv_i(FrtFilter *filt, IndexReader *ir)
{
    FrtBitVector *bv = bv_new_capa(ir->max_doc(ir));
    FrtSearcher *sea = isea_new(ir);
    FrtWeight *weight = q_weight(QF(filt)->query, sea);
    FrtScorer *scorer = weight->scorer(weight, ir);
    if (scorer) {
        while (scorer->next(scorer)) {
            bv_set(bv, scorer->doc);
        }
        scorer->destroy(scorer);
    }
    weight->destroy(weight);
    free(sea);
    return bv;
}

static unsigned long long qfilt_hash(FrtFilter *filt)
{
    return q_hash(QF(filt)->query);
}

static int qfilt_eq(FrtFilter *filt, FrtFilter *o)
{
    return q_eq(QF(filt)->query, QF(o)->query);
}

static void qfilt_destroy_i(FrtFilter *filt)
{
    Query *query = QF(filt)->query;
    q_deref(query);
    filt_destroy_i(filt);
}

FrtFilter *qfilt_new_nr(Query *query)
{
    FrtFilter *filt = filt_new(QueryFilter);

    QF(filt)->query = query;

    filt->get_bv_i  = &qfilt_get_bv_i;
    filt->hash      = &qfilt_hash;
    filt->eq        = &qfilt_eq;
    filt->to_s      = &qfilt_to_s;
    filt->destroy_i = &qfilt_destroy_i;
    return filt;
}

FrtFilter *qfilt_new(Query *query)
{
    FRT_REF(query);
    return qfilt_new_nr(query);
}
