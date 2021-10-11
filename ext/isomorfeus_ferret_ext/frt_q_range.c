#include <string.h>
#include "frt_search.h"
#include "frt_symbol.h"
#include "frt_internal.h"

/*****************************************************************************
 *
 * Range
 *
 *****************************************************************************/

typedef struct Range
{
    FrtSymbol field;
    char *lower_term;
    char *upper_term;
    bool include_lower : 1;
    bool include_upper : 1;
} Range;

static char *range_to_s(Range *range, FrtSymbol default_field, float boost)
{
    char *buffer, *b;
    size_t flen, llen, ulen;
    const char *field = range->field;

    flen = strlen(field);
    llen = range->lower_term ? strlen(range->lower_term) : 0;
    ulen = range->upper_term ? strlen(range->upper_term) : 0;
    buffer = FRT_ALLOC_N(char, flen + llen + ulen + 40);
    b = buffer;

    if ((default_field != NULL) && (strcmp(default_field, range->field) != 0)) {
        memcpy(buffer, field, flen * sizeof(char));
        b += flen;
        *b = ':';
        b++;
    }

    if (range->lower_term) {
        *b = range->include_lower ? '[' : '{';
        b++;
        memcpy(b, range->lower_term, llen);
        b += llen;
    } else {
        *b = '<';
        b++;
    }

    if (range->upper_term && range->lower_term) {
        *b = ' '; b++;
    }

    if (range->upper_term) {
        memcpy(b, range->upper_term, ulen);
        b += ulen;
        *b = range->include_upper ? ']' : '}';
        b++;
    } else {
        *b = '>';
        b++;
    }

    *b = 0;
    if (boost != 1.0) {
        *b = '^';
        frt_dbl_to_s(b + 1, boost);
    }
    return buffer;
}

static void range_destroy(Range *range)
{
    free(range->lower_term);
    free(range->upper_term);
    free(range);
}

static unsigned long long range_hash(Range *filt)
{
    return filt->include_lower | (filt->include_upper << 1)
        | ((frt_sym_hash(filt->field)
            ^ (filt->lower_term ? frt_str_hash(filt->lower_term) : 0)
            ^ (filt->upper_term ? frt_str_hash(filt->upper_term) : 0)) << 2);
}

static int range_eq(Range *filt, Range *o)
{
    if ((filt->lower_term && !o->lower_term) || (!filt->lower_term && o->lower_term)) { return false; }
    if ((filt->upper_term && !o->upper_term) || (!filt->upper_term && o->upper_term)) { return false; }
    return ((strcmp(filt->field, o->field) == 0)
            && ((filt->lower_term && o->lower_term) ? (strcmp(filt->lower_term, o->lower_term) == 0) : 1)
            && ((filt->upper_term && o->upper_term) ? (strcmp(filt->upper_term, o->upper_term) == 0) : 1)
            && (filt->include_lower == o->include_lower)
            && (filt->include_upper == o->include_upper));
}

static Range *range_new(FrtSymbol field, const char *lower_term,
                 const char *upper_term, bool include_lower,
                 bool include_upper)
{
    Range *range;

    if (!lower_term && !upper_term) {
        rb_raise(rb_eArgError, "Nil bounds for range. A range must include either "
              "lower bound or an upper bound");
    }
    if (include_lower && !lower_term) {
        rb_raise(rb_eArgError, "Lower bound must be non-nil to be inclusive. That "
              "is, if you specify :include_lower => true when you create a "
              "range you must include a :lower_term");
    }
    if (include_upper && !upper_term) {
        rb_raise(rb_eArgError, "Upper bound must be non-nil to be inclusive. That "
              "is, if you specify :include_upper => true when you create a "
              "range you must include a :upper_term");
    }
    if (upper_term && lower_term && (strcmp(upper_term, lower_term) < 0)) {
        rb_raise(rb_eArgError, "Upper bound must be greater than lower bound. "
              "\"%s\" < \"%s\"", upper_term, lower_term);
    }

    range = FRT_ALLOC(Range);

    range->field = field;
    range->lower_term = lower_term ? frt_estrdup(lower_term) : NULL;
    range->upper_term = upper_term ? frt_estrdup(upper_term) : NULL;
    range->include_lower = include_lower;
    range->include_upper = include_upper;
    return range;
}

static Range *trange_new(FrtSymbol field, const char *lower_term,
                  const char *upper_term, bool include_lower,
                  bool include_upper)
{
    Range *range;
    int len;
    double upper_num, lower_num;

    if (!lower_term && !upper_term) {
        rb_raise(rb_eArgError, "Nil bounds for range. A range must include either "
              "lower bound or an upper bound");
    }
    if (include_lower && !lower_term) {
        rb_raise(rb_eArgError, "Lower bound must be non-nil to be inclusive. That "
              "is, if you specify :include_lower => true when you create a "
              "range you must include a :lower_term");
    }
    if (include_upper && !upper_term) {
        rb_raise(rb_eArgError, "Upper bound must be non-nil to be inclusive. That "
              "is, if you specify :include_upper => true when you create a "
              "range you must include a :upper_term");
    }
    if (upper_term && lower_term) {
        if ((!lower_term ||
             (sscanf(lower_term, "%lg%n", &lower_num, &len) &&
              (int)strlen(lower_term) == len)) &&
            (!upper_term ||
             (sscanf(upper_term, "%lg%n", &upper_num, &len) &&
              (int)strlen(upper_term) == len)))
        {
            if (upper_num < lower_num) {
                rb_raise(rb_eArgError, "Upper bound must be greater than lower bound."
                      " numbers \"%lg\" < \"%lg\"", upper_num, lower_num);
            }
        }
        else {
            if (upper_term && lower_term &&
                (strcmp(upper_term, lower_term) < 0)) {
                rb_raise(rb_eArgError, "Upper bound must be greater than lower bound."
                      " \"%s\" < \"%s\"", upper_term, lower_term);
            }
        }
    }

    range = FRT_ALLOC(Range);

    range->field = field;
    range->lower_term = lower_term ? frt_estrdup(lower_term) : NULL;
    range->upper_term = upper_term ? frt_estrdup(upper_term) : NULL;
    range->include_lower = include_lower;
    range->include_upper = include_upper;
    return range;
}

/***************************************************************************
 *
 * RangeFilter
 *
 ***************************************************************************/

typedef struct RangeFilter
{
    FrtFilter super;
    Range *range;
} RangeFilter;

#define RF(filt) ((RangeFilter *)(filt))

static void rfilt_destroy_i(FrtFilter *filt)
{
    range_destroy(RF(filt)->range);
    frt_filt_destroy_i(filt);
}

static char *rfilt_to_s(FrtFilter *filt)
{
    char *rstr = range_to_s(RF(filt)->range, NULL, 1.0);
    char *rfstr = frt_strfmt("RangeFilter< %s >", rstr);
    free(rstr);
    return rfstr;
}

static FrtBitVector *rfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir)
{
    FrtBitVector *bv = frt_bv_new_capa(ir->max_doc(ir));
    Range *range = RF(filt)->range;
    FrtFieldInfo *fi = fis_get_field(ir->fis, range->field);
    /* the field info exists we need to add docs to the bit vector, otherwise
     * we just return an empty bit vector */
    if (fi) {
        const char *lower_term =
            range->lower_term ? range->lower_term : FRT_EMPTY_STRING;
        const char *upper_term = range->upper_term;
        const bool include_upper = range->include_upper;
        const int field_num = fi->number;
        char *term;
        FrtTermEnum* te;
        FrtTermDocEnum *tde;
        bool check_lower;

        te = ir->terms(ir, field_num);
        if (te->skip_to(te, lower_term) == NULL) {
            te->close(te);
            return bv;
        }

        check_lower = !(range->include_lower || (lower_term == FRT_EMPTY_STRING));

        tde = ir->term_docs(ir);
        term = te->curr_term;
        do {
            if (!check_lower
                || (strcmp(term, lower_term) > 0)) {
                check_lower = false;
                if (upper_term) {
                    int compare = strcmp(upper_term, term);
                    /* Break if upper term is greater than or equal to upper
                     * term and include_upper is false or ther term is fully
                     * greater than upper term. This is optimized so that only
                     * one check is done except in last check or two */
                    if ((compare <= 0)
                        && (!include_upper || (compare < 0))) {
                        break;
                    }
                }
                /* we have a good term, find the docs */
                /* text is already pointing to term buffer text */
                tde->seek_te(tde, te);
                while (tde->next(tde)) {
                    frt_bv_set(bv, tde->doc_num(tde));
                    /* printf("Setting %d\n", tde->doc_num(tde)); */
                }
            }
        } while (te->next(te));

        tde->close(tde);
        te->close(te);
    }

    return bv;
}

static unsigned long long rfilt_hash(FrtFilter *filt)
{
    return range_hash(RF(filt)->range);
}

static int rfilt_eq(FrtFilter *filt, FrtFilter *o)
{
    return range_eq(RF(filt)->range, RF(o)->range);
}

FrtFilter *frt_rfilt_new(FrtSymbol field,
                  const char *lower_term, const char *upper_term,
                  bool include_lower, bool include_upper)
{
    FrtFilter *filt = filt_new(RangeFilter);
    RF(filt)->range =  range_new(field, lower_term, upper_term,
                                 include_lower, include_upper);

    filt->get_bv_i  = &rfilt_get_bv_i;
    filt->hash      = &rfilt_hash;
    filt->eq        = &rfilt_eq;
    filt->to_s      = &rfilt_to_s;
    filt->destroy_i = &rfilt_destroy_i;
    return filt;
}

/***************************************************************************
 *
 * RangeFilter
 *
 ***************************************************************************/

static char *trfilt_to_s(FrtFilter *filt)
{
    char *rstr = range_to_s(RF(filt)->range, NULL, 1.0);
    char *rfstr = frt_strfmt("TypedRangeFilter< %s >", rstr);
    free(rstr);
    return rfstr;
}

typedef enum {
    TRC_NONE    = 0x00,
    TRC_LE      = 0x01,
    TRC_LT      = 0x02,
    TRC_GE      = 0x04,
    TRC_GE_LE   = 0x05,
    TRC_GE_LT   = 0x06,
    TRC_GT      = 0x08,
    TRC_GT_LE   = 0x09,
    TRC_GT_LT   = 0x0a
} TypedRangeCheck;

#define SET_DOCS(cond)\
do {\
    if (term[0] > '9') break; /* done */\
    sscanf(term, "%lg%n", &num, &len);\
    if (len == te->curr_term_len) { /* We have a number */\
        if (cond) {\
            tde->seek_te(tde, te);\
            while (tde->next(tde)) {\
                frt_bv_set(bv, tde->doc_num(tde));\
            }\
        }\
    }\
} while (te->next(te))


static FrtBitVector *trfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir)
{
    Range *range = RF(filt)->range;
    double lnum = 0.0, unum = 0.0;
    int len = 0;
    const char *lt = range->lower_term;
    const char *ut = range->upper_term;
    if ((!lt || (sscanf(lt, "%lg%n", &lnum, &len) && (int)strlen(lt) == len)) &&
        (!ut || (sscanf(ut, "%lg%n", &unum, &len) && (int)strlen(ut) == len)))
    {
        FrtBitVector *bv = frt_bv_new_capa(ir->max_doc(ir));
        FrtFieldInfo *fi = fis_get_field(ir->fis, range->field);
        /* the field info exists we need to add docs to the bit vector,
         * otherwise we just return an empty bit vector */
        if (fi) {
            const int field_num = fi->number;
            char *term;
            double num;
            FrtTermEnum* te;
            FrtTermDocEnum *tde;
            TypedRangeCheck check = TRC_NONE;

            te = ir->terms(ir, field_num);
            if (te->skip_to(te, "+.") == NULL) {
                te->close(te);
                return bv;
            }

            tde = ir->term_docs(ir);
            term = te->curr_term;

            if (lt) {
                check = range->include_lower ? TRC_GE : TRC_GT;
            }
            if (ut) {
               check = (TypedRangeCheck)(check | (range->include_upper
                                                  ? TRC_LE
                                                  : TRC_LT));
            }

            switch(check) {
                case TRC_LE:
                    SET_DOCS(num <= unum);
                    break;
                case TRC_LT:
                    SET_DOCS(num <  unum);
                    break;
                case TRC_GE:
                    SET_DOCS(num >= lnum);
                    break;
                case TRC_GE_LE:
                    SET_DOCS(num >= lnum && num <= unum);
                    break;
                case TRC_GE_LT:
                    SET_DOCS(num >= lnum && num <  unum);
                    break;
                case TRC_GT:
                    SET_DOCS(num >  lnum);
                    break;
                case TRC_GT_LE:
                    SET_DOCS(num >  lnum && num <= unum);
                    break;
                case TRC_GT_LT:
                    SET_DOCS(num >  lnum && num <  unum);
                    break;
                case TRC_NONE:
                    /* should never happen. Error should have been rb_raised */
                    assert(false);
            }
            tde->close(tde);
            te->close(te);
        }

        return bv;
    }
    else {
        return rfilt_get_bv_i(filt, ir);
    }
}

FrtFilter *frt_trfilt_new(FrtSymbol field,
                   const char *lower_term, const char *upper_term,
                   bool include_lower, bool include_upper)
{
    FrtFilter *filt = filt_new(RangeFilter);
    RF(filt)->range =  trange_new(field, lower_term, upper_term,
                                  include_lower, include_upper);

    filt->get_bv_i  = &trfilt_get_bv_i;
    filt->hash      = &rfilt_hash;
    filt->eq        = &rfilt_eq;
    filt->to_s      = &trfilt_to_s;
    filt->destroy_i = &rfilt_destroy_i;
    return filt;
}

/*****************************************************************************
 *
 * RangeQuery
 *
 *****************************************************************************/

#define RQ(query) ((FrtRangeQuery *)(query))
typedef struct FrtRangeQuery
{
    FrtQuery f;
    Range *range;
} FrtRangeQuery;

static char *rq_to_s(FrtQuery *self, FrtSymbol field)
{
    return range_to_s(RQ(self)->range, field, self->boost);
}

static void rq_destroy(FrtQuery *self)
{
    range_destroy(RQ(self)->range);
    q_destroy_i(self);
}

static FrtMatchVector *rq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv,
                                    FrtTermVector *tv)
{
    Range *range = RQ(((FrtConstantScoreQuery *)self)->original)->range;
    if (strcmp(tv->field, range->field) == 0) {
        const int term_cnt = tv->term_cnt;
        int i, j;
        char *upper_text = range->upper_term;
        char *lower_text = range->lower_term;
        int upper_limit = range->include_upper ? 1 : 0;

        i = lower_text ? frt_tv_scan_to_term_index(tv, lower_text) : 0;
        if (i < term_cnt && !range->include_lower && lower_text
            && 0 == strcmp(lower_text, tv->terms[i].text)) {
            i++;
        }

        for (; i < term_cnt; i++) {
            FrtTVTerm *tv_term = &(tv->terms[i]);
            char *text = tv_term->text;
            const int tv_term_freq = tv_term->freq;
            if (upper_text && strcmp(text, upper_text) >= upper_limit) {
                break;
            }
            for (j = 0; j < tv_term_freq; j++) {
                int pos = tv_term->positions[j];
                matchv_add(mv, pos, pos);
            }
        }
    }
    return mv;
}

static FrtQuery *rq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    FrtQuery *csq;
    Range *r = RQ(self)->range;
    FrtFilter *filter = frt_rfilt_new(r->field, r->lower_term, r->upper_term,
                               r->include_lower, r->include_upper);
    (void)ir;
    csq = frt_csq_new_nr(filter);
    ((FrtConstantScoreQuery *)csq)->original = self;
    csq->get_matchv_i = &rq_get_matchv_i;
    return (FrtQuery *)csq;
}

static unsigned long long rq_hash(FrtQuery *self)
{
    return range_hash(RQ(self)->range);
}

static int rq_eq(FrtQuery *self, FrtQuery *o)
{
    return range_eq(RQ(self)->range, RQ(o)->range);
}

FrtQuery *frt_rq_new(FrtSymbol field, const char *lower_term,
              const char *upper_term, bool include_lower, bool include_upper)
{
    FrtQuery *self;
    Range *range            = range_new(field, lower_term, upper_term,
                                        include_lower, include_upper);
    self                    = q_new(FrtRangeQuery);
    RQ(self)->range         = range;

    self->type              = RANGE_QUERY;
    self->rewrite           = &rq_rewrite;
    self->to_s              = &rq_to_s;
    self->hash              = &rq_hash;
    self->eq                = &rq_eq;
    self->destroy_i         = &rq_destroy;
    self->create_weight_i   = &q_create_weight_unsup;
    return self;
}

/*****************************************************************************
 *
 * TypedRangeQuery
 *
 *****************************************************************************/

#define SET_TERMS(cond)\
for (i = tv->term_cnt - 1; i >= 0; i--) {\
    FrtTVTerm *tv_term = &(tv->terms[i]);\
    char *text = tv_term->text;\
    double num;\
    sscanf(text, "%lg%n", &num, &len);\
    if ((int)strlen(text) == len) { /* We have a number */\
        if (cond) {\
            const int tv_term_freq = tv_term->freq;\
            for (j = 0; j < tv_term_freq; j++) {\
                int pos = tv_term->positions[j];\
                matchv_add(mv, pos, pos);\
            }\
        }\
    }\
}\

static FrtMatchVector *trq_get_matchv_i(FrtQuery *self, FrtMatchVector *mv,
                                     FrtTermVector *tv)
{
    Range *range = RQ(((FrtConstantScoreQuery *)self)->original)->range;
    if (strcmp(tv->field, range->field) == 0) {
        double lnum = 0.0, unum = 0.0;
        int len = 0;
        const char *lt = range->lower_term;
        const char *ut = range->upper_term;
        if ((!lt
             || (sscanf(lt,"%lg%n",&lnum,&len) && (int)strlen(lt) == len))
            &&
            (!ut
             || (sscanf(ut,"%lg%n",&unum,&len) && (int)strlen(ut) == len)))
        {
            TypedRangeCheck check = TRC_NONE;
            int i = 0, j = 0;

            if (lt) {
               check = range->include_lower ? TRC_GE : TRC_GT;
            }
            if (ut) {
               check = (TypedRangeCheck)(check | (range->include_upper
                                                  ? TRC_LE
                                                  : TRC_LT));
            }

            switch(check) {
                case TRC_LE:
                    SET_TERMS(num <= unum);
                    break;
                case TRC_LT:
                    SET_TERMS(num <  unum);
                    break;
                case TRC_GE:
                    SET_TERMS(num >= lnum);
                    break;
                case TRC_GE_LE:
                    SET_TERMS(num >= lnum && num <= unum);
                    break;
                case TRC_GE_LT:
                    SET_TERMS(num >= lnum && num <  unum);
                    break;
                case TRC_GT:
                    SET_TERMS(num >  lnum);
                    break;
                case TRC_GT_LE:
                    SET_TERMS(num >  lnum && num <= unum);
                    break;
                case TRC_GT_LT:
                    SET_TERMS(num >  lnum && num <  unum);
                    break;
                case TRC_NONE:
                    /* should never happen. Error should have been rb_raised */
                    assert(false);
            }

        }
        else {
            return rq_get_matchv_i(self, mv, tv);
        }
    }
    return mv;
}

static FrtQuery *trq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    FrtQuery *csq;
    Range *r = RQ(self)->range;
    FrtFilter *filter = frt_trfilt_new(r->field, r->lower_term, r->upper_term,
                                r->include_lower, r->include_upper);
    (void)ir;
    csq = frt_csq_new_nr(filter);
    ((FrtConstantScoreQuery *)csq)->original = self;
    csq->get_matchv_i = &trq_get_matchv_i;
    return (FrtQuery *)csq;
}

FrtQuery *frt_trq_new(FrtSymbol field, const char *lower_term,
               const char *upper_term, bool include_lower, bool include_upper)
{
    FrtQuery *self;
    Range *range            = trange_new(field, lower_term, upper_term,
                                         include_lower, include_upper);
    self                    = q_new(FrtRangeQuery);
    RQ(self)->range         = range;

    self->type              = TYPED_RANGE_QUERY;
    self->rewrite           = &trq_rewrite;
    self->to_s              = &rq_to_s;
    self->hash              = &rq_hash;
    self->eq                = &rq_eq;
    self->destroy_i         = &rq_destroy;
    self->create_weight_i   = &q_create_weight_unsup;
    return self;
}
