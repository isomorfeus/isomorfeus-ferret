#include <string.h>
#include "frt_search.h"
#include "frt_symbol.h"
#include "frt_internal.h"

/****************************************************************************
 *
 * FrtPrefixQuery
 *
 ****************************************************************************/

#define PfxQ(query) ((FrtPrefixQuery *)(query))

static char *prq_to_s(FrtQuery *self, FrtSymbol default_field)
{
    char *buffer, *bptr;
    const char *prefix = PfxQ(self)->prefix;
    size_t plen = strlen(prefix);
    size_t flen = strlen(PfxQ(self)->field);

    bptr = buffer = FRT_ALLOC_N(char, plen + flen + 35);

    if (default_field != NULL && strcmp(PfxQ(self)->field, default_field) != 0) {
        bptr += sprintf(bptr, "%s:", PfxQ(self)->field);
    }

    bptr += sprintf(bptr, "%s*", prefix);
    if (self->boost != 1.0) {
        *bptr = '^';
        frt_dbl_to_s(++bptr, self->boost);
    }

    return buffer;
}

static FrtQuery *prq_rewrite(FrtQuery *self, FrtIndexReader *ir)
{
    const int field_num = fis_get_field_num(ir->fis, PfxQ(self)->field);
    FrtQuery *volatile q = frt_multi_tq_new_conf(PfxQ(self)->field,
                                          FrtMTQMaxTerms(self), 0.0);
    q->boost = self->boost;        /* set the boost */

    if (field_num >= 0) {
        const char *prefix = PfxQ(self)->prefix;
        FrtTermEnum *te = ir->terms_from(ir, field_num, prefix);
        const char *term = te->curr_term;
        size_t prefix_len = strlen(prefix);

        FRT_TRY
            do {
                if (strncmp(term, prefix, prefix_len) != 0) {
                    break;
                }
                frt_multi_tq_add_term(q, term);       /* found a match */
            } while (te->next(te));
        FRT_XFINALLY
            te->close(te);
        FRT_XENDTRY
    }

    return q;
}

static void prq_destroy(FrtQuery *self)
{
    free(PfxQ(self)->prefix);
    frt_q_destroy_i(self);
}

static unsigned long long prq_hash(FrtQuery *self)
{
    return frt_sym_hash(PfxQ(self)->field) ^ frt_str_hash(PfxQ(self)->prefix);
}

static int prq_eq(FrtQuery *self, FrtQuery *o)
{
    return (strcmp(PfxQ(self)->prefix, PfxQ(o)->prefix) == 0)
        && (strcmp(PfxQ(self)->field, PfxQ(o)->field) == 0);
}

FrtQuery *frt_prefixq_new(FrtSymbol field, const char *prefix)
{
    FrtQuery *self = frt_q_new(FrtPrefixQuery);

    PfxQ(self)->field       = field;
    PfxQ(self)->prefix      = frt_estrdup(prefix);
    FrtMTQMaxTerms(self)       = PREFIX_QUERY_MAX_TERMS;

    self->type              = PREFIX_QUERY;
    self->rewrite           = &prq_rewrite;
    self->to_s              = &prq_to_s;
    self->hash              = &prq_hash;
    self->eq                = &prq_eq;
    self->destroy_i         = &prq_destroy;
    self->create_weight_i   = &frt_q_create_weight_unsup;

    return self;
}
