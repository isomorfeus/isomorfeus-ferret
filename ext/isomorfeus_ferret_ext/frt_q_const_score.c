#include "frt_search.h"
#include <string.h>
#include "frt_internal.h"

/***************************************************************************
 *
 * ConstantScoreScorer
 *
 ***************************************************************************/

#define CScQ(query) ((FrtConstantScoreQuery *)(query))
#define CScSc(scorer) ((ConstantScoreScorer *)(scorer))

typedef struct ConstantScoreScorer
{
    FrtScorer      super;
    FrtBitVector  *bv;
    float       score;
} ConstantScoreScorer;

static float cssc_score(FrtScorer *self)
{
    return CScSc(self)->score;
}

static bool cssc_next(FrtScorer *self)
{
    return ((self->doc = bv_scan_next(CScSc(self)->bv)) >= 0);
}

static bool cssc_skip_to(FrtScorer *self, int doc_num)
{
    return ((self->doc = bv_scan_next_from(CScSc(self)->bv, doc_num)) >= 0);
}

static FrtExplanation *cssc_explain(FrtScorer *self, int doc_num)
{
    (void)self; (void)doc_num;
    return expl_new(1.0, "ConstantScoreScorer");
}

static FrtScorer *cssc_new(FrtWeight *weight, FrtIndexReader *ir)
{
    FrtScorer *self    = scorer_new(ConstantScoreScorer, weight->similarity);
    FrtFilter *filter  = CScQ(weight->query)->filter;

    CScSc(self)->score  = weight->value;
    CScSc(self)->bv     = filt_get_bv(filter, ir);

    self->score     = &cssc_score;
    self->next      = &cssc_next;
    self->skip_to   = &cssc_skip_to;
    self->explain   = &cssc_explain;
    self->destroy   = &scorer_destroy_i;
    return self;
}

/***************************************************************************
 *
 * ConstantScoreWeight
 *
 ***************************************************************************/

static char *csw_to_s(FrtWeight *self)
{
    return strfmt("ConstantScoreWeight(%f)", self->value);
}

static FrtExplanation *csw_explain(FrtWeight *self, FrtIndexReader *ir, int doc_num)
{
    FrtFilter *filter = CScQ(self->query)->filter;
    FrtExplanation *expl;
    char *filter_str = filter->to_s(filter);
    FrtBitVector *bv = filt_get_bv(filter, ir);

    if (bv_get(bv, doc_num)) {
        expl = expl_new(self->value,
                        "ConstantScoreQuery(%s), product of:", filter_str);
        expl_add_detail(expl, expl_new(self->query->boost, "boost"));
        expl_add_detail(expl, expl_new(self->qnorm, "query_norm"));
    }
    else {
        expl = expl_new(self->value,
                        "ConstantScoreQuery(%s), does not match id %d",
                        filter_str, doc_num);
    }
    free(filter_str);
    return expl;
}

static FrtWeight *csw_new(FrtQuery *query, FrtSearcher *searcher)
{
    FrtWeight *self        = w_new(FrtWeight, query);

    self->scorer        = &cssc_new;
    self->explain       = &csw_explain;
    self->to_s          = &csw_to_s;

    self->similarity    = query->get_similarity(query, searcher);
    self->idf           = 1.0f;

    return self;
}

/***************************************************************************
 *
 * ConstantScoreQuery
 *
 ***************************************************************************/

static char *csq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    FrtFilter *filter = CScQ(self)->filter;
    char *filter_str = filter->to_s(filter);
    char *buffer;
    (void)default_field;
    if (self->boost == 1.0) {
        buffer = strfmt("ConstantScore(%s)", filter_str);
    }
    else {
        buffer = strfmt("ConstantScore(%s)^%f", filter_str, self->boost);
    }
    free(filter_str);
    return buffer;;
}

static void csq_destroy(FrtQuery *self)
{
    filt_deref(CScQ(self)->filter);
    q_destroy_i(self);
}

static unsigned long long csq_hash(FrtQuery *self)
{
    return filt_hash(CScQ(self)->filter);
}

static int csq_eq(FrtQuery *self, FrtQuery *o)
{
    return filt_eq(CScQ(self)->filter, CScQ(o)->filter);
}

FrtQuery *csq_new_nr(FrtFilter *filter)
{
    FrtQuery *self = q_new(FrtConstantScoreQuery);
    CScQ(self)->filter = filter;

    self->type              = CONSTANT_QUERY;
    self->to_s              = &csq_to_s;
    self->hash              = &csq_hash;
    self->eq                = &csq_eq;
    self->destroy_i         = &csq_destroy;
    self->create_weight_i   = &csw_new;

    return self;
}

FrtQuery *csq_new(FrtFilter *filter)
{
    FRT_REF(filter);
    return csq_new_nr(filter);
}
