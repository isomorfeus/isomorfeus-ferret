#include <ctype.h>
#include "frt_array.h"
#include "frt_search.h"
#include "isomorfeus_ferret.h"
#include <ruby.h>

// #undef close

VALUE mSearch;

static VALUE cHit;
static VALUE cTopDocs;
static VALUE cExplanation;
static VALUE cSearcher;
static VALUE cMultiSearcher;
static VALUE cSortField;
static VALUE cSort;

/* Queries */
static VALUE cQuery;
static VALUE cTermQuery;
static VALUE cMultiTermQuery;
static VALUE cBooleanQuery;
static VALUE cBooleanClause;
static VALUE cRangeQuery;
static VALUE cTypedRangeQuery;
static VALUE cPhraseQuery;
static VALUE cPrefixQuery;
static VALUE cWildcardQuery;
static VALUE cFuzzyQuery;
static VALUE cMatchAllQuery;
static VALUE cConstantScoreQuery;
static VALUE cFilteredQuery;
static VALUE cSpanTermQuery;
static VALUE cSpanMultiTermQuery;
static VALUE cSpanPrefixQuery;
static VALUE cSpanFirstQuery;
static VALUE cSpanNearQuery;
static VALUE cSpanOrQuery;
static VALUE cSpanNotQuery;

/* Filters */
static ID id_bits;
static VALUE cFilter;
static VALUE cRangeFilter;
static VALUE cTypedRangeFilter;
static VALUE cQueryFilter;

/* MultiTermQuery */
static ID id_default_max_terms;
static VALUE sym_max_terms;
static VALUE sym_min_score;

/** Option hash keys **/
/* BooleanClause */
static VALUE sym_should;
static VALUE sym_must;
static VALUE sym_must_not;

/* RangeQuery */
static VALUE sym_upper;
static VALUE sym_lower;
static VALUE sym_include_upper;
static VALUE sym_include_lower;
static VALUE sym_upper_exclusive;
static VALUE sym_lower_exclusive;

static VALUE sym_less_than;
static VALUE sym_less_than_or_equal_to;
static VALUE sym_greater_than;
static VALUE sym_greater_than_or_equal_to;

/* FuzzyQuery */
static VALUE sym_min_similarity;
static VALUE sym_prefix_length;

/* SpanNearQuery */
static VALUE sym_slop;
static VALUE sym_in_order;
static VALUE sym_clauses;

/* Class variable ids */
static ID id_default_min_similarity;
static ID id_default_prefix_length;


/** Sort **/
static VALUE oSORT_FIELD_DOC;

/* Sort types */
static VALUE sym_integer;
static VALUE sym_float;
static VALUE sym_string;
static VALUE sym_auto;
static VALUE sym_doc_id;
static VALUE sym_score;
static VALUE sym_byte;

/* Sort params */
static VALUE sym_type;
static VALUE sym_reverse;
static VALUE sym_comparator;

/* Hits */
static ID id_doc;
static ID id_score;

/* TopDocs */
static ID id_hits;
static ID id_total_hits;
static ID id_max_score;
static ID id_searcher;

/* Search */
static VALUE sym_offset;
static VALUE sym_limit;
static VALUE sym_start_doc;
static VALUE sym_all;
static VALUE sym_sort;
static VALUE sym_filter;
static VALUE sym_filter_proc;
static VALUE sym_c_filter_proc;

static VALUE sym_excerpt_length;
static VALUE sym_num_excerpts;
static VALUE sym_pre_tag;
static VALUE sym_post_tag;
static VALUE sym_ellipsis;

static ID fsym_id;

extern VALUE cIndexReader;
extern const rb_data_type_t frb_index_reader_t;

extern void frb_set_term(VALUE rterm, FrtTerm *t);
extern VALUE frb_get_analyzer(FrtAnalyzer *a);
extern FrtHashSet *frb_get_fields(VALUE rfields);
extern FrtAnalyzer *frb_get_cwrapped_analyzer(VALUE ranalyzer);
extern VALUE frb_get_lazy_doc(FrtLazyDoc *lazy_doc);

/****************************************************************************
 *
 * FrtHit Methods
 *
 ****************************************************************************/

static VALUE frb_get_hit(FrtHit *hit) {
    return rb_struct_new(cHit, INT2FIX(hit->doc), rb_float_new((double)hit->score), NULL);
}

/****************************************************************************
 *
 * TopDocs Methods
 *
 ****************************************************************************/

static VALUE frb_get_td(FrtTopDocs *td, VALUE rsearcher) {
    int i;
    VALUE rtop_docs;
    VALUE hit_ary = rb_ary_new2(td->size);

    for (i = 0; i < td->size; i++) {
      rb_ary_store(hit_ary, i, frb_get_hit(td->hits[i]));
    }

    rtop_docs = rb_struct_new(cTopDocs, INT2FIX(td->total_hits), hit_ary,
                              rb_float_new((double)td->max_score), rsearcher, NULL);
    frt_td_destroy(td);
    return rtop_docs;
}

/*
 *  call-seq:
 *     top_doc.to_s(field = :id) -> string
 *
 *  Returns a string representation of the top_doc in readable format.
 */
static VALUE frb_td_to_s(int argc, VALUE *argv, VALUE self) {
    int i;
    VALUE rhits = rb_funcall(self, id_hits, 0);
    FrtSearcher *sea = (FrtSearcher *)DATA_PTR(rb_funcall(self, id_searcher, 0));
    const int len = RARRAY_LEN(rhits);
    unsigned int capa = len * 64 + 100;
    int p = 0;
    char *str = FRT_ALLOC_N(char, len * 64 + 100);
    ID field = fsym_id;
    VALUE rstr;

    if (argc) {
        field = frb_field(argv[0]);
    }

    sprintf(str, "TopDocs: total_hits = %ld, max_score = %lf [\n",
            FIX2LONG(rb_funcall(self, id_total_hits, 0)),
            NUM2DBL(rb_funcall(self, id_max_score, 0)));
    p = (int)strlen(str);

    for (i = 0; i < len; i++) {
        VALUE rhit = RARRAY_PTR(rhits)[i];
        int doc_id = FIX2INT(rb_funcall(rhit, id_doc, 0));
        const char *value = "";
        size_t value_len = 0;
        FrtLazyDoc *lzd = sea->get_lazy_doc(sea, doc_id);
        FrtLazyDocField *lzdf = frt_lazy_doc_get(lzd, field);
        if (NULL != lzdf) {
            value = frt_lazy_df_get_data(lzdf, 0);
            value_len = strlen(value);
        }
        if (p + value_len + 64 > capa) {
            capa += (value_len + 64) * (len - i);
            FRT_REALLOC_N(str, char, capa);
        }

        sprintf(str + p, "\t%d \"%s\": %0.5f\n", doc_id, value,
                NUM2DBL(rb_funcall(rhit, id_score, 0)));
        p += strlen(str + p);
        frt_lazy_doc_close(lzd);
    }

    sprintf(str + p, "]\n");
    rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

static char *frb_lzd_load_to_json(FrtLazyDoc *lzd, char **str, char *s, int *slen) {
	int i, j;
	int diff = s - *str;
	int len = diff, l;
	FrtLazyDocField *f;

	for (i = 0; i < lzd->size; i++) {
		f = lzd->fields[i];
        /* 3 times length of field to make space for quoted quotes ('"') and
         * 4 times field elements to make space for '"' around fields and ','
         * between fields. Add 100 for '[', ']' and good safety.
         */
        len += strlen(rb_id2name(f->name)) + f->len * 3 + 100 + 4 * f->size;
    }

    if (len > *slen) {
        while (len > *slen) *slen = *slen << 1;
        FRT_REALLOC_N(*str, char, *slen);
        s = *str + diff;
    }

	for (i = 0; i < lzd->size; i++) {
        const char *field_name;
		f = lzd->fields[i];
        field_name = rb_id2name(f->name);
		if (i)  *(s++) = ',';
        *(s++) = '"';
        l = strlen(field_name);
        memcpy(s, field_name, l);
        s += l;
        *(s++) = '"';
        *(s++) = ':';
        if (f->size > 1)  *(s++) = '[';
		for (j = 0; j < f->size; j++) {
			if (j) *(s++) = ',';
			s = json_concat_string(s, frt_lazy_df_get_data(f, j));
		}
        if (f->size > 1)  *(s++) = ']';
	}
	return s;
}

/*
 *  call-seq:
 *     top_doc.to_json() -> string
 *
 *  Returns a json representation of the top_doc.
 */
static VALUE frb_td_to_json(VALUE self) {
	int i;
	VALUE rhits = rb_funcall(self, id_hits, 0);
	VALUE rhit;
	FrtLazyDoc *lzd;
	FrtSearcher *sea = (FrtSearcher *)DATA_PTR(rb_funcall(self, id_searcher, 0));
	const int num_hits = RARRAY_LEN(rhits);
	int doc_id;
    int len = 32768;
	char *str = FRT_ALLOC_N(char, len);
    char *s = str;
	VALUE rstr;

    *(s++) = '[';
	for (i = 0; i < num_hits; i++) {
        if (i) *(s++) = ',';
        *(s++) = '{';
		rhit = RARRAY_PTR(rhits)[i];
		doc_id = FIX2INT(rb_funcall(rhit, id_doc, 0));
		lzd = sea->get_lazy_doc(sea, doc_id);
		s = frb_lzd_load_to_json(lzd, &str, s, &len);
        frt_lazy_doc_close(lzd);
        *(s++) = '}';
	}
    *(s++) = ']';
    *(s++) = '\0';
	rstr = rb_str_new2(str);
	free(str);
	return rstr;
}


/****************************************************************************
 *
 * Explanation Methods
 *
 ****************************************************************************/

#define GET_EXPL() FrtExplanation *expl = (FrtExplanation *)DATA_PTR(self)

/*
 *  call-seq:
 *     explanation.to_s -> string
 *
 *  Returns a string representation of the explanation in readable format.
 */
static VALUE frb_expl_to_s(VALUE self) {
    GET_EXPL();
    char *str = frt_expl_to_s(expl);
    VALUE rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

/*
 *  call-seq:
 *     explanation.to_html -> string
 *
 *  Returns an html representation of the explanation in readable format.
 */
static VALUE frb_expl_to_html(VALUE self) {
    GET_EXPL();
    char *str = frt_expl_to_html(expl);
    VALUE rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

/*
 *  call-seq:
 *     explanation.score -> float
 *
 *  Returns the score represented by the query. This can be used for debugging
 *  purposes mainly to check that the score returned by the explanation
 *  matches that of the score for the document in the original query.
 */
static VALUE frb_expl_score(VALUE self) {
    GET_EXPL();
    return rb_float_new((double)expl->value);
}

/****************************************************************************
 *
 * Query Methods
 *
 ****************************************************************************/

static void frb_q_free(void *p) {
    frt_q_deref((FrtQuery *)p);
}

/*
 *  call-seq:
 *     query.to_s -> string
 *
 *  Return a string representation of the query. Most of the time, passing
 *  this string through the Query parser will give you the exact Query you
 *  began with. This can be a good way to explore how the QueryParser works.
 */
static VALUE frb_q_to_s(int argc, VALUE *argv, VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    VALUE rstr, rfield;
    char *str;
    ID field = (ID)NULL;
    if (rb_scan_args(argc, argv, "01", &rfield)) {
        field = frb_field(rfield);
    }
    str = q->to_s(q, field);
    rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

/*
 *  call-seq:
 *     query.boost
 *
 *  Returns the queries boost value. See the Query description for more
 *  information on Query boosts.
 */
static VALUE frb_q_get_boost(VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    return rb_float_new((double)q->boost);
}

/*
 *  call-seq:
 *     query.boost = boost -> boost
 *
 *  Set the boost for a query. See the Query description for more information
 *  on Query boosts.
 */
static VALUE frb_q_set_boost(VALUE self, VALUE rboost) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    q->boost = (float)NUM2DBL(rboost);
    return rboost;
}

/*
 *  call-seq:
 *     query.hash -> number
 *
 *  Return a hash value for the query. This is used for caching query results
 *  in a hash object.
 */
static VALUE frb_q_hash(VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    return INT2FIX(q->hash(q));
}

/*
 *  call-seq;
 *     query.eql?(other_query) -> bool
 *     query == other_query -> bool
 *
 *  Return true if +query+ equals +other_query+. Theoretically, two queries are
 *  equal if the always return the same results, no matter what the contents
 *  of the index. Practically, however, this is difficult to implement
 *  efficiently for queries like BooleanQuery since the ordering of clauses
 *  unspecified. "Ruby AND Rails" will not match "Rails AND Ruby" for example,
 *  although their result sets will be identical. Most queries should match as
 *  expected however.
 */
static VALUE frb_q_eql(VALUE self, VALUE other) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    FrtQuery *oq;
    oq = DATA_PTR(other);
    return q->eq(q, oq) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     query.terms(searcher) -> term_array
 *
 *  Returns an array of terms searched for by this query. This can be used for
 *  implementing an external query highlighter for example. You must supply a
 *  searcher so that the query can be rewritten and optimized like it would be
 *  in a real search.
 */
static VALUE frb_q_get_terms(VALUE self, VALUE searcher) {
    VALUE rterms = rb_ary_new();
    FrtHashSet *terms = frt_hs_new((frt_hash_ft)&frt_term_hash,
                            (frt_eq_ft)&frt_term_eq,
                            (frt_free_ft)frt_term_destroy);
    FrtHashSetEntry *hse;
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    FrtSearcher *sea = (FrtSearcher *)DATA_PTR(searcher);
    FrtQuery *rq = sea->rewrite(sea, q);
    rq->extract_terms(rq, terms);
    frt_q_deref(rq);

    for (hse = terms->first; hse; hse = hse->next) {
        FrtTerm *term = (FrtTerm *)hse->elem;
        rb_ary_push(rterms, frb_get_term(term->field, term->text));
    }
    frt_hs_destroy(terms);
    return rterms;
}

/****************************************************************************
 * TermQuery Methods
 ****************************************************************************/

static size_t frb_term_query_size(const void *p) {
    return sizeof(FrtTermQuery);
    (void)p;
}

const rb_data_type_t frb_term_query_t = {
    .wrap_struct_name = "FrbTermQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_term_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_tq_alloc(VALUE rclass) {
    FrtQuery *tq = frt_tq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_term_query_t, tq);
}

typedef struct frb_tq_args {
    VALUE self;
    VALUE rfield;
    VALUE rterm;
    char  *term;
    ID    field;
} frb_tq_args;

VALUE frb_tq_init_2(VALUE args) {
    frb_tq_args *a = (frb_tq_args *)args;
    a->field = frb_field(a->rfield);
    a->term = rs2s(rb_obj_as_string(a->rterm));
    return Qnil;
}

VALUE frb_q_init_r(VALUE args, VALUE exception) {
    frb_tq_args *a = (frb_tq_args *)args;
    free(((struct RData *)(a->self))->data);
    ((struct RData *)(a->self))->data = NULL;
    ((struct RData *)(a->self))->dmark = NULL;
    ((struct RData *)(a->self))->dfree = NULL;
    rb_exc_raise(exception);
    return Qnil;
}

/*
 *  call-seq:
 *     TermQuery.new(field, term) -> term_query
 *
 *  Create a new TermQuery object which will match all documents with the term
 *  +term+ in the field +field+.
 *
 *  Note: As usual, field should be a symbol
 */
static VALUE frb_tq_init(VALUE self, VALUE rfield, VALUE rterm) {
    frb_tq_args args = { .self = self, .rfield = rfield, .rterm = rterm, .field = 0, .term = NULL };
    rb_rescue(frb_tq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_term_query_t, q);
    frt_tq_init(q, args.field, args.term);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * MultiTermQuery Methods
 *
 ****************************************************************************/

static size_t frb_multi_term_query_size(const void *p) {
    return sizeof(FrtMultiTermQuery);
    (void)p;
}

const rb_data_type_t frb_multi_term_query_t = {
    .wrap_struct_name = "FrbTermQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_multi_term_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_mtq_alloc(VALUE rclass) {
    FrtQuery *tq = frt_multi_tq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_multi_term_query_t, tq);
}

/*
 *  call-seq:
 *     MultiTermQuery.default_max_terms -> number
 *
 *  Get the default value for +:max_terms+ in a MultiTermQuery. This value is
 *  also used by PrefixQuery, FuzzyQuery and WildcardQuery.
 */
static VALUE frb_mtq_get_dmt(VALUE self) {
    return rb_cvar_get(cMultiTermQuery, id_default_max_terms);
}

/*
 *  call-seq:
 *     MultiTermQuery.default_max_terms = max_terms -> max_terms
 *
 *  Set the default value for +:max_terms+ in a MultiTermQuery. This value is
 *  also used by PrefixQuery, FuzzyQuery and WildcardQuery.
 */
static VALUE frb_mtq_set_dmt(VALUE self, VALUE rnum_terms) {
    int max_terms = FIX2INT(rnum_terms);
    if (max_terms <= 0) {
        rb_raise(rb_eArgError, "%d <= 0. @@max_terms must be > 0", max_terms);
    }
    rb_cvar_set(cMultiTermQuery, id_default_max_terms, rnum_terms);
    return rnum_terms;
}

typedef struct frb_mtq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    float min_score;
    int   max_terms;
    ID    field;
} frb_mtq_args;

VALUE frb_mtq_init_2(VALUE args) {
    frb_mtq_args *a = (frb_mtq_args *)args;
    VALUE rfield, roptions;
    a->min_score = 0.0f;
    a->max_terms = FIX2INT(frb_mtq_get_dmt(a->self));
    if (rb_scan_args(a->argc, a->argv, "11", &rfield, &roptions) == 2) {
        VALUE v;
        if (Qnil != (v = rb_hash_aref(roptions, sym_max_terms))) {
            a->max_terms = FIX2INT(v);
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_min_score))) {
            a->min_score = (float)NUM2DBL(v);
        }
    }
    a->field = frb_field(rfield);
    return Qnil;
}

/*
 *  call-seq:
 *     MultiTermQuery.new(field, options = {}) -> multi_term_query
 *
 *  Create a new MultiTermQuery on field +field+. You will also need to add
 *  terms to the query using the MultiTermQuery#add_term method.
 *
 *  There are several options available to you when creating a
 *  MultiTermQueries;
 *
 *  === Options
 *
 *  :max_terms:: You can specify the maximum number of terms that can be
 *               added to the query. This is to prevent memory usage overflow,
 *               particularly when don't directly control the addition of
 *               terms to the Query object like when you create Wildcard
 *               queries. For example, searching for "content:*" would cause
 *               problems without this limit.
 *  :min_score:: The minimum score a term must have to be added to the query.
 *               For example you could implement your own wild-card queries
 *               that gives matches a score. To limit the number of terms
 *               added to the query you could set a lower limit to this score.
 *               FuzzyQuery in particular makes use of this parameter.
 */
static VALUE frb_mtq_init(int argc, VALUE *argv, VALUE self) {
    frb_mtq_args args = { .self = self, .argc = argc, .argv = argv, .field = 0 };
    rb_rescue(frb_mtq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_multi_term_query_t, q);
    frt_multi_tq_init_conf(q, args.field, args.max_terms, args.min_score);
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *     multi_term_query.add_term(term, score = 1.0) -> self
 *     multi_term_query << term1 << term2 << term3 -> self
 *
 *  Add a term to the MultiTermQuery with the score 1.0 unless specified
 *  otherwise.
 */
static VALUE frb_mtq_add_term(int argc, VALUE *argv, VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    VALUE rterm, rboost;
    float boost = 1.0f;
    char *term = NULL;
    if (rb_scan_args(argc, argv, "11", &rterm, &rboost) == 2) {
        boost = (float)NUM2DBL(rboost);
    }
    term = StringValuePtr(rterm);
    frt_multi_tq_add_term_boost(q, term, boost);

    return self;
}

typedef FrtQuery *(*mtq_maker_ft)(ID field, const char *term);

static int get_max_terms(VALUE rmax_terms, int max_terms) {
    VALUE v;
    switch (TYPE(rmax_terms)) {
        case T_HASH:
            if (Qnil != (v = rb_hash_aref(rmax_terms, sym_max_terms))) {
                max_terms = FIX2INT(v);
            }
            break;
        case T_FIXNUM:
            max_terms = FIX2INT(rmax_terms);
            break;
        default:
            rb_raise(rb_eArgError, "max_terms must be an integer");
    }
    return max_terms;
}

/****************************************************************************
 *
 * BooleanClause Methods
 *
 ****************************************************************************/

static size_t frb_boolean_clause_t_size(const void *p) {
    return sizeof(FrtBooleanClause);
    (void)p;
}

static void frb_bc_mark(void *p) {
    if (((FrtBooleanClause *)p)->query && ((FrtBooleanClause *)p)->query->rquery)
        rb_gc_mark(((FrtBooleanClause *)p)->query->rquery);
}

static void frb_bc_free(void *p) {
    frt_bc_deref((FrtBooleanClause *)p);
}

const rb_data_type_t frb_boolean_clause_t = {
    .wrap_struct_name = "FrbBooleanClause",
    .function = {
        .dmark = frb_bc_mark,
        .dfree = frb_bc_free,
        .dsize = frb_boolean_clause_t_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_bc_alloc(VALUE rclass) {
    FrtBooleanClause *bc = frt_bc_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_boolean_clause_t, bc);
}

static VALUE frb_bc_wrap(FrtBooleanClause *bc) {
    VALUE self = TypedData_Wrap_Struct(cBooleanClause, &frb_boolean_clause_t, bc);
    bc->rbc = self;
    FRT_REF(bc);
    return self;
}

static FrtBCType frb_get_occur(VALUE roccur) {
    FrtBCType occur = FRT_BC_SHOULD;

    if (roccur == sym_should) {
        occur = FRT_BC_SHOULD;
    } else if (roccur == sym_must) {
        occur = FRT_BC_MUST;
    } else if (roccur == sym_must_not) {
        occur = FRT_BC_MUST_NOT;
    } else {
        rb_raise(rb_eArgError, "occur argument must be one of [:must, :should, :must_not]");
    }
    return occur;
}

/*
 *  call-seq:
 *     BooleanClause.new(query, occur = :should) -> BooleanClause
 *
 *  Create a new BooleanClause object, wrapping the query +query+. +occur+
 *  must be one of +:must+, +:should+ or +:must_not+.
 */
static VALUE frb_bc_init(int argc, VALUE *argv, VALUE self) {
    FrtBooleanClause *bc;
    TypedData_Get_Struct(self, FrtBooleanClause, &frb_boolean_clause_t, bc);
    VALUE rquery, roccur;
    unsigned int occur = FRT_BC_SHOULD;
    FrtQuery *sub_q;
    if (rb_scan_args(argc, argv, "11", &rquery, &roccur) == 2) {
        occur = frb_get_occur(roccur);
    }
    sub_q = DATA_PTR(rquery);
    FRT_REF(sub_q);
    frt_bc_init(bc, sub_q, occur);
    bc->rbc = self;
    return self;
}

/*
 *  call-seq:
 *     clause.query -> query
 *
 *  Return the query object wrapped by this BooleanClause.
 */
static VALUE frb_bc_get_query(VALUE self) {
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    if (bc->query->rquery)
        return bc->query->rquery;
    return Qnil;
}

/*
 *  call-seq:
 *     clause.query = query -> query
 *
 *  Set the query wrapped by this BooleanClause.
 */
static VALUE frb_bc_set_query(VALUE self, VALUE rquery) {
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    bc->query = DATA_PTR(rquery);
    return rquery;
}

/*
 *  call-seq:
 *     clause.required? -> bool
 *
 *  Return true if this clause is required. ie, this will be true if occur was
 *  equal to +:must+.
 */
static VALUE frb_bc_is_required(VALUE self) {
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    return bc->is_required ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     clause.prohibited? -> bool
 *
 *  Return true if this clause is prohibited. ie, this will be true if occur was
 *  equal to +:must_not+.
 */
static VALUE frb_bc_is_prohibited(VALUE self) {
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    return bc->is_prohibited ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     clause.occur = occur -> occur
 *
 *  Set the +occur+ value for this BooleanClause. +occur+ must be one of
 *  +:must+, +:should+ or +:must_not+.
 */
static VALUE frb_bc_set_occur(VALUE self, VALUE roccur) {
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    FrtBCType occur = frb_get_occur(roccur);
    frt_bc_set_occur(bc, occur);

    return roccur;
}

/*
 *  call-seq:
 *     clause.to_s -> string
 *
 *  Return a string representation of this clause. This will not be used by
 *  BooleanQuery#to_s. It is only used by BooleanClause#to_s and will specify
 *  whether the clause is +:must+, +:should+ or +:must_not+.
 */
static VALUE frb_bc_to_s(VALUE self) {
    VALUE rstr;
    char *qstr, *str;
    const char *ostr = "";
    int len;
    FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(self);
    qstr = bc->query->to_s(bc->query, (ID)NULL);
    switch (bc->occur) {
        case FRT_BC_SHOULD:
            ostr = "Should";
            break;
        case FRT_BC_MUST:
            ostr = "Must";
            break;
        case FRT_BC_MUST_NOT:
            ostr = "Must Not";
            break;
    }
    len = strlen(ostr) + strlen(qstr) + 2;
    str = FRT_ALLOC_N(char, len);
    sprintf(str, "%s:%s", ostr, qstr);
    rstr = rb_str_new(str, len);
    free(qstr);
    free(str);
    return rstr;
}

/****************************************************************************
 *
 * BooleanQuery Methods
 *
 ****************************************************************************/

static size_t frb_boolean_query_size(const void *p) {
    return sizeof(FrtBooleanQuery);
    (void)p;
}

static void frb_bq_mark(void *p) {
    int i;
    FrtQuery *q = (FrtQuery *)p;
    FrtBooleanQuery *bq = (FrtBooleanQuery *)q;
    for (i = 0; i < bq->clause_cnt; i++) {
        if (bq->clauses[i]->rbc)
            rb_gc_mark(bq->clauses[i]->rbc);
    }
}

const rb_data_type_t frb_boolean_query_t = {
    .wrap_struct_name = "FrbBooleanQuery",
    .function = {
        .dmark = frb_bq_mark,
        .dfree = frb_q_free,
        .dsize = frb_boolean_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_bq_alloc(VALUE rclass) {
    FrtQuery *bq = frt_bq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_boolean_query_t, bq);
}

typedef struct frb_bq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    bool coord_disabled;
} frb_bq_args;

VALUE frb_bq_init_2(VALUE args) {
    frb_bq_args *a = (frb_bq_args *)args;
    VALUE rcoord_disabled;
    a->coord_disabled = false;
    if (rb_scan_args(a->argc, a->argv, "01", &rcoord_disabled)) {
        a->coord_disabled = RTEST(rcoord_disabled);
    }
    return Qnil;
}

/*
 *  call-seq:
 *     BooleanQuery.new(coord_disable = false)
 *
 *  Create a new BooleanQuery. If you don't care about the scores of the
 *  sub-queries added to the query (as would be the case for many
 *  automatically generated queries) you can disable the coord_factor of the
 *  score. This will slightly improve performance for the query. Usually you
 *  should leave this parameter as is.
 */
static VALUE frb_bq_init(int argc, VALUE *argv, VALUE self) {
    frb_bq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_bq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_boolean_query_t, q);
    frt_bq_init(q, args.coord_disabled);
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *     boolean_query.add_query(query, occur = :should) -> boolean_clause
 *     boolean_query.<<(query, occur = :should) -> boolean_clause
 *     boolean_query << boolean_clause -> boolean_clause
 *
 *  Us this method to add sub-queries to a BooleanQuery. You can either add
 *  a straight Query or a BooleanClause. When adding a Query, the default
 *  occurrence requirement is :should. That is the Query's match will be
 *  scored but it isn't essential for a match. If the query should be
 *  essential, use :must. For exclusive queries use :must_not.
 *
 *  When adding a Boolean clause to a BooleanQuery there is no need to set the
 *  occurrence property because it is already set in the BooleanClause.
 *  Therefor the +occur+ parameter will be ignored in this case.
 *
 *  query::   Query to add to the BooleanQuery
 *  occur::   occurrence requirement for the query being added. Must be one of
 *            [:must, :should, :must_not]
 *  returns:: BooleanClause which was added
 */
static VALUE frb_bq_add_query(int argc, VALUE *argv, VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    VALUE rquery, roccur;
    FrtBCType occur = FRT_BC_SHOULD;
    FrtQuery *sub_q;
    VALUE klass;

    if (rb_scan_args(argc, argv, "11", &rquery, &roccur) == 2) {
        occur = frb_get_occur(roccur);
    }
    klass = CLASS_OF(rquery);
    if (klass == cBooleanClause) {
        FrtBooleanClause *bc = (FrtBooleanClause *)DATA_PTR(rquery);
        if (argc > 1) {
            rb_warning("Second argument to BooleanQuery#add is ignored when adding BooleanClause");
        }
        frt_bq_add_clause(q, bc);
        return rquery;
    } else if (TYPE(rquery) == T_DATA) {
        sub_q = DATA_PTR(rquery);
        return frb_bc_wrap(frt_bq_add_query(q, sub_q, occur));
    } else {
        rb_raise(rb_eArgError, "Cannot add %s to a BooleanQuery", rb_class2name(klass));
    }
    return self;
}

/****************************************************************************
 *
 * RangeQuery Methods
 *
 ****************************************************************************/

static size_t frb_range_query_size(const void *p) {
    return sizeof(FrtRangeQuery);
    (void)p;
}

const rb_data_type_t frb_range_query_t = {
    .wrap_struct_name = "FrbRangeQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_range_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_rq_alloc(VALUE rclass) {
    FrtQuery *rq = frt_rq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_range_query_t, rq);
}

typedef struct frb_rq_args {
    VALUE self;
    VALUE rfield;
    VALUE roptions;
    ID    field;
    char  *lterm;
    char  *uterm;
    bool  include_lower;
    bool  include_upper;
} frb_rq_args;

VALUE frb_rq_init_2(VALUE args) {
    frb_rq_args *a = (frb_rq_args *)args;
    VALUE v;
    Check_Type(a->roptions, T_HASH);
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_lower))) {
        a->lterm = rs2s(rb_obj_as_string(v));
        a->include_lower = true;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_upper))) {
        a->uterm = rs2s(rb_obj_as_string(v));
        a->include_upper = true;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_lower_exclusive))) {
        a->lterm = rs2s(rb_obj_as_string(v));
        a->include_lower = false;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_upper_exclusive))) {
        a->uterm = rs2s(rb_obj_as_string(v));
        a->include_upper = false;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_include_lower))) {
        a->include_lower = RTEST(v);
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_include_upper))) {
        a->include_upper = RTEST(v);
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_greater_than))) {
        a->lterm = rs2s(rb_obj_as_string(v));
        a->include_lower = false;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_greater_than_or_equal_to))) {
        a->lterm = rs2s(rb_obj_as_string(v));
        a->include_lower = true;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_less_than))) {
        a->uterm = rs2s(rb_obj_as_string(v));
        a->include_upper = false;
    }
    if (Qnil != (v = rb_hash_aref(a->roptions, sym_less_than_or_equal_to))) {
        a->uterm = rs2s(rb_obj_as_string(v));
        a->include_upper = true;
    }
    if (!a->lterm && !a->uterm) {
        rb_raise(rb_eArgError, "The bounds of a range should not both be nil");
    }
    if (a->include_lower && !a->lterm) {
        rb_raise(rb_eArgError, "The lower bound should not be nil if it is inclusive");
    }
    if (a->include_upper && !a->uterm) {
        rb_raise(rb_eArgError, "The upper bound should not be nil if it is inclusive");
    }
    a->field = frb_field(a->rfield);
    return Qnil;
}

/*
 *  call-seq:
 *     RangeQuery.new(field, options = {}) -> range_query
 *
 *  Create a new RangeQuery on field +field+. There are two ways to build a
 *  range query. With the old-style options; +:lower+, +:upper+,
 *  +:include_lower+ and +:include_upper+ or the new style options; +:<+,
 *  +:<=+, +:>+ and +:>=+. The options' names should speak for themselves.
 *  In the old-style options, limits are inclusive by default.
 *
 *  == Examples
 *
 *    q = RangeQuery.new(:date, :lower => "200501", :include_lower => false)
 *    # is equivalent to
 *    q = RangeQuery.new(:date, :< => "200501")
 *    # is equivalent to
 *    q = RangeQuery.new(:date, :lower_exclusive => "200501")
 *
 *    q = RangeQuery.new(:date, :lower => "200501", :upper => 200502)
 *    # is equivalent to
 *    q = RangeQuery.new(:date, :>= => "200501", :<= => 200502)
 *
 */
static VALUE frb_rq_init(VALUE self, VALUE rfield, VALUE roptions) {
    frb_rq_args a = { .self = self, .rfield = rfield, .roptions = roptions };
    rb_rescue(frb_rq_init_2, (VALUE)&a, frb_q_init_r, (VALUE)&a);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_range_query_t, q);
    frt_rq_init(q, a.field, a.lterm, a.uterm, a.include_lower, a.include_upper);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * TypedRangeQuery Methods
 *
 ****************************************************************************/

static size_t frb_typed_range_query_size(const void *p) {
    return sizeof(FrtRangeQuery);
    (void)p;
}

const rb_data_type_t frb_typed_range_query_t = {
    .wrap_struct_name = "FrbTypedRangeQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_typed_range_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_trq_alloc(VALUE rclass) {
    FrtQuery *trq = frt_trq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_typed_range_query_t, trq);
}

/*
 *  call-seq:
 *     TypedRangeQuery.new(field, options = {}) -> range_query
 *
 *  Create a new TypedRangeQuery on field +field+. This differs from the
 *  standard RangeQuery in that it allows range queries with unpadded numbers,
 *  both positive and negative, integer and float. You can even use
 *  hexadecimal numbers. However it could be a lot slower than the standard
 *  RangeQuery on large indexes.
 *
 *  There are two ways to build a range query. With the old-style options;
 *  +:lower+, +:upper+, +:include_lower+ and +:include_upper+ or the new style
 *  options; +:<+, +:<=+, +:>+ and +:>=+. The options' names should speak for
 *  themselves.  In the old-style options, limits are inclusive by default.
 *
 *  == Examples
 *
 *    q = TypedRangeQuery.new(:date, :lower => "0.1", :include_lower => false)
 *    # is equivalent to
 *    q = TypedRangeQuery.new(:date, :< => "0.1")
 *    # is equivalent to
 *    q = TypedRangeQuery.new(:date, :lower_exclusive => "0.1")
 *
 *    # Note that you numbers can be strings or actual numbers
 *    q = TypedRangeQuery.new(:date, :lower => "-12.32", :upper => 0.21)
 *    # is equivalent to
 *    q = TypedRangeQuery.new(:date, :>= => "-12.32", :<= => 0.21)
 */
static VALUE frb_trq_init(VALUE self, VALUE rfield, VALUE roptions) {
    frb_rq_args a = { .self = self, .rfield = rfield, .roptions = roptions };
    rb_rescue(frb_rq_init_2, (VALUE)&a, frb_q_init_r, (VALUE)&a);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_typed_range_query_t, q);
    q = frt_trq_init(q, a.field, a.lterm, a.uterm, a.include_lower, a.include_upper);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * PhraseQuery Methods
 *
 ****************************************************************************/

static size_t frb_phrase_query_size(const void *p) {
    return sizeof(FrtPhraseQuery);
    (void)p;
}

const rb_data_type_t frb_phrase_query_t = {
    .wrap_struct_name = "FrbPhraseQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_phrase_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_phq_alloc(VALUE rclass) {
    FrtQuery *phq = frt_phq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_phrase_query_t, phq);
}

typedef struct frb_phq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    ID    field;
    int   slop;
} frb_phq_args;

VALUE frb_phq_init_2(VALUE args) {
    frb_phq_args *a = (frb_phq_args *)args;
    VALUE rfield, rslop;
    rb_scan_args(a->argc, a->argv, "11", &rfield, &rslop);
    a->field = frb_field(rfield);
    if (rslop != Qnil) {
        a->slop = FIX2INT(rslop);
    }
    return Qnil;
}

/*
 *  call-seq:
 *     PhraseQuery.new(field, slop = 0) -> phrase_query
 *
 *  Create a new PhraseQuery on the field +field+. You need to add terms to
 *  the query it will do anything of value. See PhraseQuery#add_term.
 */
static VALUE frb_phq_init(int argc, VALUE *argv, VALUE self) {
    frb_phq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_phq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_phrase_query_t, q);
    frt_phq_init(q, args.field);
    if (argc == 2) {
        ((FrtPhraseQuery *)q)->slop = args.slop;
    }
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *    phrase_query.add_term(term, position_increment = 1) -> phrase_query
 *    phrase_query << term -> phrase_query
 *
 *  Add a term to the phrase query. By default the position_increment is set
 *  to 1 so each term you add is expected to come directly after the previous
 *  term. By setting position_increment to 2 you are specifying that the term
 *  you just added should occur two terms after the previous term. For
 *  example;
 *
 *    phrase_query.add_term("big").add_term("house", 2)
 *    # matches => "big brick house"
 *    # matches => "big red house"
 *    # doesn't match => "big house"
 */
static VALUE
frb_phq_add(int argc, VALUE *argv, VALUE self)
{
    VALUE rterm, rpos_inc;
    int pos_inc = 1;
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    if (rb_scan_args(argc, argv, "11", &rterm, &rpos_inc) == 2) {
        pos_inc = FIX2INT(rpos_inc);
    }
    switch (TYPE(rterm)) {
        case T_STRING:
            {
                frt_phq_add_term(q, StringValuePtr(rterm), pos_inc);
                break;
            }
        case T_ARRAY:
            {
                int i;
                char *t;
                if (RARRAY_LEN(rterm) < 1) {
                    rb_raise(rb_eArgError, "Cannot add empty array to a "
                             "PhraseQuery. You must add either a string or "
                             "an array of strings");
                }
                t = StringValuePtr(RARRAY_PTR(rterm)[0]);
                frt_phq_add_term(q, t, pos_inc);
                for (i = 1; i < RARRAY_LEN(rterm); i++) {
                    t = StringValuePtr(RARRAY_PTR(rterm)[i]);
                    frt_phq_append_multi_term(q, t);
                }
                break;
            }
        default:
            rb_raise(rb_eArgError, "You can only add a string or an array of "
                     "strings to a PhraseQuery, not a %s\n",
                     rs2s(rb_obj_as_string(rterm)));
    }
    return self;
}

/*
 *  call-seq:
 *     phrase_query.slop -> integer
 *
 *  Return the slop set for this phrase query. See the PhraseQuery
 *  description for more information on slop
 */
static VALUE
frb_phq_get_slop(VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    return INT2FIX(((FrtPhraseQuery *)q)->slop);
}

/*
 *  call-seq:
 *     phrase_query.slop = slop -> slop
 *
 *  Set the slop set for this phrase query. See the PhraseQuery description
 *  for more information on slop
 */
static VALUE
frb_phq_set_slop(VALUE self, VALUE rslop) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    ((FrtPhraseQuery *)q)->slop = FIX2INT(rslop);
    return self;
}

/****************************************************************************
 *
 * PrefixQuery Methods
 *
 ****************************************************************************/

static size_t frb_prefix_query_size(const void *p) {
    return sizeof(FrtPrefixQuery);
    (void)p;
}

const rb_data_type_t frb_prefix_query_t = {
    .wrap_struct_name = "FrbPrefixQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_prefix_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_prq_alloc(VALUE rclass) {
    FrtQuery *pq = frt_prefixq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_prefix_query_t, pq);
}

typedef struct frb_prq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    ID    field;
    char  *term;
    int   max_terms;
} frb_prq_args;

VALUE frb_prq_init_2(VALUE args) {
    frb_prq_args *a = (frb_prq_args *)args;
    VALUE rfield, rterm, rmax_terms;
    a->max_terms = FIX2INT(rb_cvar_get(cMultiTermQuery, id_default_max_terms));
    if (rb_scan_args(a->argc, a->argv, "21", &rfield, &rterm, &rmax_terms) == 3) {
        a->max_terms = get_max_terms(rmax_terms, a->max_terms);
    }
    a->field = frb_field(rfield);
    a->term = StringValuePtr(rterm);
    return Qnil;
}

/*
 *  call-seq:
 *     PrefixQuery.new(field, prefix, options = {}) -> prefix-query
 *
 *  Create a new PrefixQuery to search for all terms with the prefix +prefix+
 *  in the field +field+. There is one option that you can set to change the
 *  behaviour of this query. +:max_terms+ specifies the maximum number of
 *  terms to be added to the query when it is expanded into a MultiTermQuery.
 *  Let's say for example you search an index with a million terms for all
 *  terms beginning with the letter "s". You would end up with a very large
 *  query which would use a lot of memory and take a long time to get results,
 *  not to mention that it would probably match every document in the index.
 *  To prevent queries like this crashing your application you can set
 *  +:max_terms+ which limits the number of terms that get added to the query.
 *  By default it is set to 512.
 */
static VALUE frb_prq_init(int argc, VALUE *argv, VALUE self) {
    frb_prq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_prq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_prefix_query_t, q);
    frt_prefixq_init(q, args.field, args.term);
    FrtMTQMaxTerms(q) = args.max_terms;
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * WildcardQuery Methods
 *
 ****************************************************************************/

static size_t frb_wildcard_query_size(const void *p) {
    return sizeof(FrtWildCardQuery);
    (void)p;
}

const rb_data_type_t frb_wilcard_query_t = {
    .wrap_struct_name = "FrbWildcardQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_wildcard_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_wcq_alloc(VALUE rclass) {
    FrtQuery *wq = frt_wcq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_wilcard_query_t, wq);
}

typedef struct frb_wcq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    ID    field;
    char  *term;
    int   max_terms;
} frb_wcq_args;

VALUE frb_wcq_init_2(VALUE args) {
    frb_wcq_args *a = (frb_wcq_args *)args;
    VALUE rfield, rterm, rmax_terms;
    a->max_terms = FIX2INT(rb_cvar_get(cMultiTermQuery, id_default_max_terms));
    if (rb_scan_args(a->argc, a->argv, "21", &rfield, &rterm, &rmax_terms) == 3) {
        a->max_terms = get_max_terms(rmax_terms, a->max_terms);
    }
    a->field = frb_field(rfield);
    a->term = StringValuePtr(rterm);
    return Qnil;
}

/*
 *  call-seq:
 *     WildcardQuery.new(field, pattern, options = {}) -> wild-card-query
 *
 *  Create a new WildcardQuery to search for all terms where the pattern
 *  +pattern+ matches in the field +field+.
 *
 *  There is one option that you can set to change the behaviour of this
 *  query. +:max_terms+ specifies the maximum number of terms to be added to
 *  the query when it is expanded into a MultiTermQuery.  Let's say for
 *  example you have a million terms in your index and you let your users do
 *  wild-card queries and one runs a search for "*". You would end up with a
 *  very large query which would use a lot of memory and take a long time to
 *  get results, not to mention that it would probably match every document in
 *  the index. To prevent queries like this crashing your application you can
 *  set +:max_terms+ which limits the number of terms that get added to the
 *  query.  By default it is set to 512.
 */
static VALUE frb_wcq_init(int argc, VALUE *argv, VALUE self) {
    frb_wcq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_wcq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_wilcard_query_t, q);
    frt_wcq_init(q, args.field, args.term);
    FrtMTQMaxTerms(q) = args.max_terms;
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * FuzzyQuery Methods
 *
 ****************************************************************************/

static size_t frb_fuzzy_query_size(const void *p) {
    return sizeof(FrtFuzzyQuery);
    (void)p;
}

const rb_data_type_t frb_fuzzy_query_t = {
    .wrap_struct_name = "FrbFuzzyQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_fuzzy_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_fq_alloc(VALUE rclass) {
    FrtQuery *fq = frt_fuzq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_fuzzy_query_t, fq);
}

typedef struct frb_fq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    ID    field;
    char  *term;
    float min_sim;
    int   max_terms;
    int   pre_len;
} frb_fq_args;

VALUE frb_fq_init_2(VALUE args) {
    frb_fq_args *a = (frb_fq_args *)args;
    VALUE rfield, rterm, roptions;
    a->min_sim = (float)NUM2DBL(rb_cvar_get(cFuzzyQuery, id_default_min_similarity));
    a->pre_len = FIX2INT(rb_cvar_get(cFuzzyQuery, id_default_prefix_length));
    a->max_terms = FIX2INT(rb_cvar_get(cMultiTermQuery, id_default_max_terms));

    if (rb_scan_args(a->argc, a->argv, "21", &rfield, &rterm, &roptions) >= 3) {
        VALUE v;
        Check_Type(roptions, T_HASH);
        if (Qnil != (v = rb_hash_aref(roptions, sym_prefix_length))) {
            a->pre_len = FIX2INT(v);
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_min_similarity))) {
            a->min_sim = (float)NUM2DBL(v);
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_max_terms))) {
            a->max_terms = FIX2INT(v);
        }
    }

    if (a->min_sim >= 1.0f) {
        rb_raise(rb_eArgError, "%f >= 1.0. :min_similarity must be < 1.0", a->min_sim);
    } else if (a->min_sim < 0.0f) {
        rb_raise(rb_eArgError, "%f < 0.0. :min_similarity must be > 0.0", a->min_sim);
    }
    if (a->pre_len < 0) {
        rb_raise(rb_eArgError, "%d < 0. :prefix_length must be >= 0", a->pre_len);
    }
    if (a->max_terms < 0) {
        rb_raise(rb_eArgError, "%d < 0. :max_terms must be >= 0", a->max_terms);
    }

    a->field = frb_field(rfield);
    a->term = StringValuePtr(rterm);
    return Qnil;
}

/*
 *  call-seq:
 *     FuzzyQuery.new(field, term, options = {}) -> fuzzy-query
 *
 *  Create a new FuzzyQuery that will match terms with a similarity of at
 *  least +:min_similarity+ to +term+. Similarity is scored using the
 *  Levenshtein edit distance formula. See
 *  http://en.wikipedia.org/wiki/Levenshtein_distance
 *
 *  If a +:prefix_length+ > 0 is specified, a common prefix of that length is
 *  also required.
 *
 *  You can also set +:max_terms+ to prevent memory overflow problems. By
 *  default it is set to 512.
 *
 *  == Example
 *
 *    FuzzyQuery.new(:content, "levenshtein",
 *                   :min_similarity => 0.8,
 *                   :prefix_length => 5,
 *                   :max_terms => 1024)
 *
 *  field::           field to search
 *  term::            term to search for including it's close matches
 *  :min_similarity:: Default: 0.5. minimum levenshtein distance score for a
 *                    match
 *  :prefix_length::  Default: 0. minimum prefix_match before levenshtein
 *                    distance is measured. This parameter is used to improve
 *                    performance.  With a +:prefix_length+ of 0, all terms in
 *                    the index must be checked which can be quite a
 *                    performance hit.  By setting the prefix length to a
 *                    larger number you minimize the number of terms that need
 *                    to be checked.  Even 1 will cut down the work by a
 *                    factor of about 26 depending on your character set and
 *                    the first letter.
 *  :max_terms::      Limits the number of terms that can be added to the
 *                    query when it is expanded as a MultiTermQuery. This is
 *                    not usually a problem with FuzzyQueries unless you set
 *                    +:min_similarity+ to a very low value.
 */
static VALUE frb_fq_init(int argc, VALUE *argv, VALUE self) {
    frb_fq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_fq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_fuzzy_query_t, q);
    frt_fuzq_init_conf(q, args.field, args.term, args.min_sim, args.pre_len, args.max_terms);
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *     FuzzyQuery.prefix_length -> prefix_length
 *
 *  Get the +:prefix_length+ for the query.
 */
static VALUE
frb_fq_pre_len(VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    return INT2FIX(((FrtFuzzyQuery *)q)->pre_len);
}

/*
 *  call-seq:
 *     FuzzyQuery.min_similarity -> min_similarity
 *
 *  Get the +:min_similarity+ for the query.
 */
static VALUE
frb_fq_min_sim(VALUE self) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    return rb_float_new((double)((FrtFuzzyQuery *)q)->min_sim);
}

/*
 *  call-seq:
 *     FuzzyQuery.default_min_similarity -> number
 *
 *  Get the default value for +:min_similarity+
 */
static VALUE
frb_fq_get_dms(VALUE self) {
    return rb_cvar_get(cFuzzyQuery, id_default_min_similarity);
}

extern float frt_qp_default_fuzzy_min_sim;
/*
 *  call-seq:
 *     FuzzyQuery.default_min_similarity = min_sim -> min_sim
 *
 *  Set the default value for +:min_similarity+
 */
static VALUE
frb_fq_set_dms(VALUE self, VALUE val)
{
    double min_sim = NUM2DBL(val);
    if (min_sim >= 1.0) {
        rb_raise(rb_eArgError,
                 "%f >= 1.0. :min_similarity must be < 1.0", min_sim);
    } else if (min_sim < 0.0) {
        rb_raise(rb_eArgError,
                 "%f < 0.0. :min_similarity must be > 0.0", min_sim);
    }
    frt_qp_default_fuzzy_min_sim = (float)min_sim;
    rb_cvar_set(cFuzzyQuery, id_default_min_similarity, val);
    return val;
}

/*
 *  call-seq:
 *     FuzzyQuery.default_prefix_length -> number
 *
 *  Get the default value for +:prefix_length+
 */
static VALUE
frb_fq_get_dpl(VALUE self)
{
    return rb_cvar_get(cFuzzyQuery, id_default_prefix_length);
}

extern int frt_qp_default_fuzzy_pre_len;
/*
 *  call-seq:
 *     FuzzyQuery.default_prefix_length = prefix_length -> prefix_length
 *
 *  Set the default value for +:prefix_length+
 */
static VALUE
frb_fq_set_dpl(VALUE self, VALUE val)
{
    int pre_len = FIX2INT(val);
    if (pre_len < 0) {
        rb_raise(rb_eArgError,
                 "%d < 0. :prefix_length must be >= 0", pre_len);
    }
    frt_qp_default_fuzzy_pre_len = pre_len;
    rb_cvar_set(cFuzzyQuery, id_default_prefix_length, val);
    return val;
}


/****************************************************************************
 *
 * MatchAllQuery Methods
 *
 ****************************************************************************/

static size_t frb_match_all_query_size(const void *p) {
    return sizeof(FrtQuery);
    (void)p;
}

const rb_data_type_t frb_match_all_query_t = {
    .wrap_struct_name = "FrbMatchAllQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_match_all_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_maq_alloc(VALUE rclass) {
    FrtQuery *q = frt_maq_new();
    return TypedData_Wrap_Struct(rclass, &frb_match_all_query_t, q);
}

/*
 *  call-seq:
 *     MatchAllQuery.new -> query
 *
 *  Create a query which matches all documents.
 */
static VALUE frb_maq_init(VALUE self) {
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_match_all_query_t, q);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * ConstantScoreQuery Methods
 *
 ****************************************************************************/

static size_t frb_constant_score_query_size(const void *p) {
    return sizeof(FrtConstantScoreQuery);
    (void)p;
}

const rb_data_type_t frb_constant_score_query_t = {
    .wrap_struct_name = "FrbConstantScoreQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_constant_score_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_csq_alloc(VALUE rclass) {
    FrtQuery *csq = frt_csq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_constant_score_query_t, csq);
}

/*
 *  call-seq:
 *     ConstantScoreQuery.new(filter) -> query
 *
 *  Create a ConstantScoreQuery which uses +filter+ to match documents giving
 *  each document a constant score.
 */
static VALUE frb_csq_init(VALUE self, VALUE rfilter) {
    FrtQuery *q;
    FrtFilter *filter = DATA_PTR(rfilter);
    TypedData_Get_Struct(self, FrtQuery, &frb_constant_score_query_t, q);
    frt_csq_init(q, filter);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * FilteredQuery Methods
 *
 ****************************************************************************/

static size_t frb_filtered_query_size(const void *p) {
    return sizeof(FrtFilteredQuery);
    (void)p;
}

static void frb_fqq_mark(void *p) {
    FrtFilteredQuery *fqq = (FrtFilteredQuery *)p;
    if (fqq->query && fqq->query->rquery)
        rb_gc_mark(fqq->query->rquery);
    if (fqq->filter && fqq->filter->rfilter)
        rb_gc_mark(fqq->filter->rfilter);
}

const rb_data_type_t frb_filtered_query_t = {
    .wrap_struct_name = "FrbFilteredQuery",
    .function = {
        .dmark = frb_fqq_mark,
        .dfree = frb_q_free,
        .dsize = frb_filtered_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_fqq_alloc(VALUE rclass) {
    FrtQuery *fqq = frt_fq_alloc();
    ((FrtFilteredQuery *)fqq)->query = NULL;
    ((FrtFilteredQuery *)fqq)->filter = NULL;
    return TypedData_Wrap_Struct(rclass, &frb_filtered_query_t, fqq);
}

/*
 *  call-seq:
 *     FilteredQuery.new(query, filter) -> query
 *
 *  Create a new FilteredQuery which filters +query+ with +filter+.
 */
static VALUE frb_fqq_init(VALUE self, VALUE rquery, VALUE rfilter) {
    FrtQuery *q;
    FrtFilter *f = DATA_PTR(rfilter);
    FrtQuery *sq = DATA_PTR(rquery);
    TypedData_Get_Struct(self, FrtQuery, &frb_filtered_query_t, q);
    frt_fq_init(q, sq, f);
    FRT_REF(sq);
    FRT_REF(f);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * SpanTermQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_term_query_size(const void *p) {
    return sizeof(FrtSpanTermQuery);
    (void)p;
}

const rb_data_type_t frb_span_term_query_t = {
    .wrap_struct_name = "FrbSpanTermQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_span_term_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spantq_alloc(VALUE rclass) {
    FrtQuery *stq = frt_spantq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_span_term_query_t, stq);
}

typedef struct frb_spantq_args {
    VALUE self;
    VALUE rfield;
    VALUE rterm;
    ID    field;
    char  *term;
} frb_spantq_args;

VALUE frb_spantq_init_2(VALUE args) {
    frb_spantq_args *a = (frb_spantq_args *)args;
    a->field = frb_field(a->rfield);
    a->term = StringValuePtr(a->rterm);
    return Qnil;
}

/*
 *  call-seq:
 *     SpanTermQuery.new(field, term) -> query
 *
 *  Create a new SpanTermQuery which matches all documents with the term
 *  +term+ in the field +field+.
 */
static VALUE frb_spantq_init(VALUE self, VALUE rfield, VALUE rterm) {
    frb_spantq_args args = { .self = self, .rfield = rfield, .rterm = rterm };
    rb_rescue(frb_spantq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_span_term_query_t, q);
    frt_spantq_init(q, args.field, args.term);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * SpanMultiTermQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_multi_term_query_size(const void *p) {
    return sizeof(FrtSpanMultiTermQuery);
    (void)p;
}

const rb_data_type_t frb_span_multi_term_query_t = {
    .wrap_struct_name = "FrbSpanMultiTermQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_span_multi_term_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spanmtq_alloc(VALUE rclass) {
    FrtQuery *smtq = frt_spanmtq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_span_multi_term_query_t, smtq);
}

typedef struct frb_spanmtq_args {
    VALUE self;
    VALUE rfield;
    ID    field;
} frb_spanmtq_args;

VALUE frb_spanmtq_init_2(VALUE args) {
    frb_spanmtq_args *a = (frb_spanmtq_args *)args;
    a->field = frb_field(a->rfield);
    return Qnil;
}

/*
 *  call-seq:
 *     SpanMultiTermQuery.new(field, terms) -> query
 *
 *  Create a new SpanMultiTermQuery which matches all documents with the terms
 *  +terms+ in the field +field+. +terms+ should be an array of Strings.
 */
static VALUE frb_spanmtq_init(VALUE self, VALUE rfield, VALUE rterms) {
    frb_spanmtq_args args = { .self = self, .rfield = rfield };
    rb_rescue(frb_spanmtq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_span_multi_term_query_t, q);
    frt_spanmtq_init(q, args.field);
    int i;
    for (i = RARRAY_LEN(rterms) - 1; i >= 0; i--) {
        frt_spanmtq_add_term(q, StringValuePtr(RARRAY_PTR(rterms)[i]));
    }
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * SpanPrefixQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_prefix_query_size(const void *p) {
    return sizeof(FrtSpanPrefixQuery);
    (void)p;
}

const rb_data_type_t frb_span_prefix_query_t = {
    .wrap_struct_name = "FrbSpanPrefixQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_span_prefix_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spanprq_alloc(VALUE rclass) {
    FrtQuery *spq = frt_spanprq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_span_prefix_query_t, spq);
}

typedef struct frb_spanprq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    ID    field;
    char  *prefix;
    int   max_terms;
} frb_spanprq_args;

VALUE frb_spanprq_init_2(VALUE args) {
    frb_spanprq_args *a = (frb_spanprq_args *)args;
    VALUE rfield, rprefix, rmax_terms;
    a->max_terms = FRT_SPAN_PREFIX_QUERY_MAX_TERMS;
    if (rb_scan_args(a->argc, a->argv, "21", &rfield, &rprefix, &rmax_terms) == 3) {
        a->max_terms = FIX2INT(rmax_terms);
    }
    a->field = frb_field(rfield);
    a->prefix = StringValuePtr(rprefix);
    return Qnil;
}

/*
 *  call-seq:
 *     SpanPrefixQuery.new(field, prefix, max_terms = 256) -> query
 *
 *  Create a new SpanPrefixQuery which matches all documents with the prefix
 *  +prefix+ in the field +field+.
 */
static VALUE frb_spanprq_init(int argc, VALUE *argv, VALUE self) {
    frb_spanprq_args args = { .self = self, .argc = argc, .argv = argv };
    rb_rescue(frb_spanprq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_span_prefix_query_t, q);
    frt_spanprq_init(q, args.field, args.prefix);
    ((FrtSpanPrefixQuery *)q)->max_terms = args.max_terms;
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * SpanFirstQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_first_query_size(const void *p) {
    return sizeof(FrtSpanFirstQuery);
    (void)p;
}

const rb_data_type_t frb_span_first_query_t = {
    .wrap_struct_name = "FrbSpanFirstQuery",
    .function = {
        .dmark = NULL,
        .dfree = frb_q_free,
        .dsize = frb_span_first_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spanfq_alloc(VALUE rclass) {
    FrtQuery *sfq = frt_spanfq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_span_first_query_t, sfq);
}

typedef struct frb_spanfq_args {
    VALUE self;
    VALUE rend;
    int end;
} frb_spanfq_args;

VALUE frb_spanfq_init_2(VALUE args) {
    frb_spanfq_args *a = (frb_spanfq_args *)args;
    a->end = FIX2INT(a->rend);
    return Qnil;
}

/*
 *  call-seq:
 *     SpanFirstQuery.new(span_query, end) -> query
 *
 *  Create a new SpanFirstQuery which matches all documents where +span_query+
 *  matches before +end+ where +end+ is a byte-offset from the start of the
 *  field
 */
static VALUE frb_spanfq_init(VALUE self, VALUE rmatch, VALUE rend) {
    frb_spanfq_args args = { .self = self, .rend = rend };
    rb_rescue(frb_spanfq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    FrtQuery *match = DATA_PTR(rmatch);
    TypedData_Get_Struct(self, FrtQuery, &frb_span_first_query_t, q);
    frt_spanfq_init(q, match, args.end);
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * SpanNearQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_near_query_size(const void *p) {
    return sizeof(FrtSpanNearQuery);
    (void)p;
}

static void frb_spannq_mark(void *p) {
    int i;
    FrtSpanNearQuery *snq = (FrtSpanNearQuery *)p;
    for (i = 0; i < snq->c_cnt; i++) {
        if (snq->clauses[i]->rquery)
            rb_gc_mark(snq->clauses[i]->rquery);
    }
}

const rb_data_type_t frb_span_near_query_t = {
    .wrap_struct_name = "FrbSpanNearQuery",
    .function = {
        .dmark = frb_spannq_mark,
        .dfree = frb_q_free,
        .dsize = frb_span_near_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spannq_alloc(VALUE rclass) {
    FrtQuery *snq = frt_spannq_alloc();
    ((FrtSpanNearQuery *)snq)->c_cnt = 0;
    return TypedData_Wrap_Struct(rclass, &frb_span_near_query_t, snq);
}

typedef struct frb_spannq_args {
    VALUE self;
    VALUE argc;
    VALUE *argv;
    int slop;
    bool in_order;
} frb_spannq_args;

VALUE frb_spannq_init_2(VALUE args) {
    frb_spannq_args *a = (frb_spannq_args *)args;
    VALUE roptions = Qnil;
    a->slop = 0;
    a->in_order = false;
    if (rb_scan_args(a->argc, a->argv, "01", &roptions) > 0) {
        VALUE v;
        if (Qnil != (v = rb_hash_aref(roptions, sym_slop))) {
            a->slop = FIX2INT(v);
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_in_order))) {
            a->in_order = RTEST(v);
        }
    }
    return roptions;
}

/*
 *  call-seq:
 *     SpanNearQuery.new(options = {}) -> query
 *
 *  Create a new SpanNearQuery. You can add an array of clauses with the
 *  +:clause+ parameter or you can add clauses individually using the
 *  SpanNearQuery#add method.
 *
 *    query = SpanNearQuery.new(:clauses => [spanq1, spanq2, spanq3])
 *    # is equivalent to
 *    query = SpanNearQuery.new()
 *    query << spanq1 << spanq2 << spanq3
 *
 *  You have two other options which you can set.
 *
 *  :slop::     Default: 0. Works exactly like a PhraseQuery slop. It is the
 *              amount of slop allowed in the match (the term edit distance
 *              allowed in the match).
 *  :in_order:: Default: false. Specifies whether or not the matches have to
 *              occur in the order they were added to the query. When slop is
 *              set to 0, this parameter will make no difference.
 */
static VALUE frb_spannq_init(int argc, VALUE *argv, VALUE self) {
    frb_spannq_args args = { .self = self, .argc = argc, .argv = argv };
    VALUE roptions = rb_rescue(frb_spannq_init_2, (VALUE)&args, frb_q_init_r, (VALUE)&args);
    FrtQuery *q;
    TypedData_Get_Struct(self, FrtQuery, &frb_span_near_query_t, q);
    frt_spannq_init(q, args.slop, args.in_order);
    if (argc > 0) {
        VALUE v;
        if (Qnil != (v = rb_hash_aref(roptions, sym_clauses))) {
            int i;
            FrtQuery *clause;
            Check_Type(v, T_ARRAY);
            for (i = 0; i < RARRAY_LEN(v); i++) {
                clause = DATA_PTR(RARRAY_PTR(v)[i]);
                frt_spannq_add_clause(q, clause);
            }
        }
    }
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *     query.add(span_query) -> self
 *     query << span_query -> self
 *
 *  Add a clause to the SpanNearQuery. Clauses are stored in the order they
 *  are added to the query which is important for matching. Note that clauses
 *  must be SpanQueries, not other types of query.
 */
static VALUE frb_spannq_add(VALUE self, VALUE rclause) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    FrtQuery *clause = DATA_PTR(rclause);
    frt_spannq_add_clause(q, clause);
    return self;
}

/****************************************************************************
 *
 * SpanOrQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_or_query_size(const void *p) {
    return sizeof(FrtSpanOrQuery);
    (void)p;
}

static void frb_spanoq_mark(void *p) {
    int i;
    FrtSpanOrQuery *soq = (FrtSpanOrQuery *)p;
    for (i = 0; i < soq->c_cnt; i++) {
        if (soq->clauses[i]->rquery)
            rb_gc_mark(soq->clauses[i]->rquery);
    }
}

const rb_data_type_t frb_span_or_query_t = {
    .wrap_struct_name = "FrbSpanOrQuery",
    .function = {
        .dmark = frb_spanoq_mark,
        .dfree = frb_q_free,
        .dsize = frb_span_or_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spanoq_alloc(VALUE rclass) {
    FrtQuery *soq = frt_spanoq_new();
    return TypedData_Wrap_Struct(rclass, &frb_span_or_query_t, soq);
}

/*
 *  call-seq:
 *     SpanOrQuery.new(options = {}) -> query
 *
 *  Create a new SpanOrQuery. This is just like a BooleanQuery with all
 *  clauses with the occur value of :should. The difference is that it can be
 *  passed to other SpanQuerys like SpanNearQuery.
 */
static VALUE frb_spanoq_init(int argc, VALUE *argv, VALUE self) {
    FrtQuery *q;
    VALUE rclauses;
    TypedData_Get_Struct(self, FrtQuery, &frb_span_or_query_t, q);
    if (rb_scan_args(argc, argv, "01", &rclauses) > 0) {
        int i;
        FrtQuery *clause;
        Check_Type(rclauses, T_ARRAY);
        for (i = 0; i < RARRAY_LEN(rclauses); i++) {
            clause = DATA_PTR(RARRAY_PTR(rclauses)[i]);
            frt_spanoq_add_clause(q, clause);
        }
    }
    q->rquery = self;
    return self;
}

/*
 *  call-seq:
 *     query.add(span_query) -> self
 *     query << span_query -> self
 *
 *  Add a clause to the SpanOrQuery. Note that clauses must be SpanQueries,
 *  not other types of query.
 */
static VALUE frb_spanoq_add(VALUE self, VALUE rclause) {
    FrtQuery *q = (FrtQuery *)DATA_PTR(self);
    FrtQuery *clause = DATA_PTR(rclause);
    frt_spanoq_add_clause(q, clause);
    return self;
}

/****************************************************************************
 *
 * SpanNotQuery Methods
 *
 ****************************************************************************/

static size_t frb_span_not_query_size(const void *p) {
    return sizeof(FrtSpanNotQuery);
    (void)p;
}

static void frb_spanxq_mark(void *p) {
    FrtSpanNotQuery *sxq = (FrtSpanNotQuery *)p;
    if (sxq->inc && sxq->inc->rquery)
        rb_gc_mark(sxq->inc->rquery);
    if (sxq->exc && sxq->exc->rquery)
        rb_gc_mark(sxq->exc->rquery);
}

const rb_data_type_t frb_span_not_query_t = {
    .wrap_struct_name = "FrbSpanNotQuery",
    .function = {
        .dmark = frb_spanxq_mark,
        .dfree = frb_q_free,
        .dsize = frb_span_not_query_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_spanxq_alloc(VALUE rclass) {
    FrtQuery *snq = frt_spanxq_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_span_not_query_t, snq);
}

/*
 *  call-seq:
 *     SpanNotQuery.new(include_query, exclude_query) -> query
 *
 *  Create a new SpanNotQuery which matches all documents which match
 *  +include_query+ and don't match +exclude_query+.
 */
static VALUE frb_spanxq_init(VALUE self, VALUE rinc, VALUE rexc) {
    FrtQuery *q;
    //Check_Type(rinc, T_DATA);
    //Check_Type(rexc, T_DATA);
    TypedData_Get_Struct(self, FrtQuery, &frb_span_not_query_t, q);
    frt_spanxq_init(q, DATA_PTR(rinc), DATA_PTR(rexc));
    q->rquery = self;
    return self;
}

/****************************************************************************
 *
 * Filter Methods
 *
 ****************************************************************************/

static void frb_f_free(void *p) {
    frt_filt_deref((FrtFilter *)p);
}

#define GET_F() FrtFilter *f = (FrtFilter *)DATA_PTR(self)

/*
 *  call-seq:
 *     filter.to_s -> string
 *
 *  Return a human readable string representing the Filter object that the
 *  method was called on.
 */
static VALUE frb_f_to_s(VALUE self) {
    VALUE rstr;
    char *str;
    GET_F();
    str = f->to_s(f);
    rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

extern VALUE frb_get_bv(FrtBitVector *bv);

/*
 *  call-seq:
 *     filter.bits(index_reader) -> bit_vector
 *
 *  Get the bit_vector used by this filter. This method will usually be used
 *  to group filters or apply filters to other filters.
 */
static VALUE frb_f_get_bits(VALUE self, VALUE rindex_reader) {
    FrtBitVector *bv;
    FrtIndexReader *ir;
    GET_F();
    TypedData_Get_Struct(rindex_reader, FrtIndexReader, &frb_index_reader_t, ir);
    bv = frt_filt_get_bv(f, ir);
    return frb_get_bv(bv);
}

/****************************************************************************
 *
 * RangeFilter Methods
 *
 ****************************************************************************/

static size_t frb_range_filter_size(const void *p) {
    return sizeof(FrtRangeFilter);
    (void)p;
}

const rb_data_type_t frb_range_filter_t = {
    .wrap_struct_name = "FrbRangeFilter",
    .function = {
        .dmark = NULL,
        .dfree = frb_f_free,
        .dsize = frb_range_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_rf_alloc(VALUE rclass) {
    FrtFilter *rf = frt_rfilt_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_range_filter_t, rf);
}

/*
 *  call-seq:
 *     RangeFilter.new(field, options = {}) -> range_query
 *
 *  Create a new RangeFilter on field +field+. There are two ways to build a
 *  range filter. With the old-style options; +:lower+, +:upper+,
 *  +:include_lower+ and +:include_upper+ or the new style options; +:<+,
 *  +:<=+, +:>+ and +:>=+. The options' names should speak for themselves.
 *  In the old-style options, limits are inclusive by default.
 *
 *  == Examples
 *
 *    f = RangeFilter.new(:date, :lower => "200501", :include_lower => false)
 *    # is equivalent to
 *    f = RangeFilter.new(:date, :< => "200501")
 *    # is equivalent to
 *    f = RangeFilter.new(:date, :lower_exclusive => "200501")
 *
 *    f = RangeFilter.new(:date, :lower => "200501", :upper => 200502)
 *    # is equivalent to
 *    f = RangeFilter.new(:date, :>= => "200501", :<= => 200502)
 */
static VALUE frb_rf_init(VALUE self, VALUE rfield, VALUE roptions) {
    int ex_code = 0;
    const char *msg = NULL;
    frb_rq_args a = { .self = self, .rfield = rfield, .roptions = roptions };
    rb_rescue(frb_rq_init_2, (VALUE)&a, frb_q_init_r, (VALUE)&a);
    FrtFilter *f;
    TypedData_Get_Struct(self, FrtFilter, &frb_range_filter_t, f);
    FRT_TRY
        frt_rfilt_init(f, a.field, a.lterm, a.uterm, a.include_lower, a.include_upper);
        f->rfilter = self;
    FRT_XCATCHALL
        ((struct RData *)(self))->data = NULL;
        ((struct RData *)(self))->dmark = NULL;
        ((struct RData *)(self))->dfree = NULL;
        free(f);
        ex_code = xcontext.excode;
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY

    if (ex_code && msg) { frb_raise(ex_code, msg); }

    return self;
}

/****************************************************************************
 *
 * TypedRangeFilter Methods
 *
 ****************************************************************************/

static size_t frb_typed_range_filter_size(const void *p) {
    return sizeof(FrtRangeFilter);
    (void)p;
}

const rb_data_type_t frb_typed_range_filter_t = {
    .wrap_struct_name = "FrbTypedRangeFilter",
    .function = {
        .dmark = NULL,
        .dfree = frb_f_free,
        .dsize = frb_typed_range_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_trf_alloc(VALUE rclass) {
    FrtFilter *f = frt_trfilt_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_typed_range_filter_t, f);
}

/*
 *  call-seq:
 *     TypedRangeFilter.new(field, options = {}) -> range_query
 *
 *  Create a new TypedRangeFilter on field +field+. There are two ways to
 *  build a range filter. With the old-style options; +:lower+, +:upper+,
 *  +:include_lower+ and +:include_upper+ or the new style options; +:<+,
 *  +:<=+, +:>+ and +:>=+. The options' names should speak for themselves.
 *  In the old-style options, limits are inclusive by default.
 *
 *  == Examples
 *
 *    f = TypedRangeFilter.new(:date, :lower => "0.1", :include_lower => false)
 *    # is equivalent to
 *    f = TypedRangeFilter.new(:date, :< => "0.1")
 *    # is equivalent to
 *    f = TypedRangeFilter.new(:date, :lower_exclusive => "0.1")
 *
 *    # Note that you numbers can be strings or actual numbers
 *    f = TypedRangeFilter.new(:date, :lower => "-132.2", :upper => -1.4)
 *    # is equivalent to
 *    f = TypedRangeFilter.new(:date, :>= => "-132.2", :<= => -1.4)
 */
static VALUE frb_trf_init(VALUE self, VALUE rfield, VALUE roptions) {
    int ex_code = 0;
    const char *msg = NULL;
    frb_rq_args a = { .self = self, .rfield = rfield, .roptions = roptions };
    rb_rescue(frb_rq_init_2, (VALUE)&a, frb_q_init_r, (VALUE)&a);
    FrtFilter *f;
    TypedData_Get_Struct(self, FrtFilter, &frb_typed_range_filter_t, f);
    FRT_TRY
        frt_trfilt_init(f, a.field, a.lterm, a.uterm, a.include_lower, a.include_upper);
        f->rfilter = self;
    FRT_XCATCHALL
        ((struct RData *)(self))->data = NULL;
        ((struct RData *)(self))->dmark = NULL;
        ((struct RData *)(self))->dfree = NULL;
        free(f);
        ex_code = xcontext.excode;
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY

    if (ex_code && msg) { frb_raise(ex_code, msg); }

    return self;
}

/****************************************************************************
 *
 * QueryFilter Methods
 *
 ****************************************************************************/

static size_t frb_query_filter_size(const void *p) {
    return sizeof(FrtQueryFilter);
    (void)p;
}

const rb_data_type_t frb_query_filter_t = {
    .wrap_struct_name = "FrbQueryFilter",
    .function = {
        .dmark = NULL,
        .dfree = frb_f_free,
        .dsize = frb_query_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_qf_alloc(VALUE rclass) {
    FrtFilter *qf = frt_qfilt_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_query_filter_t, qf);
}

/*
 *  call-seq:
 *     QueryFilter.new(query) -> filter
 *
 *  Create a new QueryFilter which applies the query +query+.
 */
static VALUE frb_qf_init(VALUE self, VALUE rquery) {
    FrtQuery *q = DATA_PTR(rquery);
    FrtFilter *f;
    TypedData_Get_Struct(self, FrtFilter, &frb_query_filter_t, f);
    frt_qfilt_init(f, q);
    f->rfilter = self;
    return self;
}

/****************************************************************************
 *
 * SortField Methods
 *
 ****************************************************************************/

static size_t frb_sort_field_size(const void *p) {
    return sizeof(FrtSortField);
    (void)p;
}

static void frb_sf_free(void *p) {
    frt_sort_field_destroy((FrtSortField *)p);
}

const rb_data_type_t frb_sort_field_t = {
    .wrap_struct_name = "FrbSortField",
    .function = {
        .dmark = NULL,
        .dfree = frb_sf_free,
        .dsize = frb_sort_field_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_sf_alloc(VALUE rclass) {
    FrtSortField *sf = frt_sort_field_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_sort_field_t, sf);
}

static VALUE frb_get_sf(FrtSortField *sf) {
    if (sf->rfield == 0 || sf->rfield == Qnil) {
        sf->rfield = TypedData_Wrap_Struct(cSortField, &frb_sort_field_t, sf);
    }
    return sf->rfield;
}

static int get_sort_type(VALUE rtype) {
    Check_Type(rtype, T_SYMBOL);
    if (rtype == sym_byte) {
        return FRT_SORT_TYPE_BYTE;
    } else if (rtype == sym_integer) {
        return FRT_SORT_TYPE_INTEGER;
    } else if (rtype == sym_string) {
        return FRT_SORT_TYPE_STRING;
    } else if (rtype == sym_score) {
        return FRT_SORT_TYPE_SCORE;
    } else if (rtype == sym_doc_id) {
        return FRT_SORT_TYPE_DOC;
    } else if (rtype == sym_float) {
        return FRT_SORT_TYPE_FLOAT;
    } else if (rtype == sym_auto) {
        return FRT_SORT_TYPE_AUTO;
    } else {
        rb_raise(rb_eArgError, ":%s is an unknown sort-type. Please choose "
                 "from [:integer, :float, :string, :auto, :score, :doc_id]",
                 rb_id2name(SYM2ID(rtype)));
    }
    return FRT_SORT_TYPE_DOC;
}

/*
 *  call-seq:
 *     SortField.new(field, options = {}) -> sort_field
 *
 *  Create a new SortField which can be used to sort the result-set by the
 *  value in field +field+.
 *
 *  === Options
 *
 *  :type::         Default: +:auto+. Specifies how a field should be sorted.
 *                  Choose from one of; +:auto+, +:integer+, +:float+,
 *                  +:string+, +:byte+, +:doc_id+ or +:score+. +:auto+ will
 *                  check the datatype of the field by trying to parse it into
 *                  either a number or a float before settling on a string
 *                  sort. String sort is locale dependent and works for
 *                  multibyte character sets like UTF-8 if you have your
 *                  locale set correctly.
 *  :reverse        Default: false. Set to true if you want to reverse the
 *                  sort.
 */
static VALUE frb_sf_init(int argc, VALUE *argv, VALUE self) {
    FrtSortField *sf;
    VALUE rfield, roptions;
    VALUE rval;
    int type = FRT_SORT_TYPE_AUTO;
    int is_reverse = false;
    ID field;
    TypedData_Get_Struct(self, FrtSortField, &frb_sort_field_t, sf);
    if (rb_scan_args(argc, argv, "11", &rfield, &roptions) == 2) {
        if (Qnil != (rval = rb_hash_aref(roptions, sym_type))) {
            type = get_sort_type(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_reverse))) {
            is_reverse = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_comparator))) {
            rb_raise(rb_eArgError, "Unsupported argument ':comparator'");
        }
    }
    if (NIL_P(rfield)) rb_raise(rb_eArgError, "must pass a valid field name");
    field = frb_field(rfield);

    frt_sort_field_init(sf, field, type, is_reverse);
    if (sf->field == (ID)NULL) {
        sf->field = field;
    }
    sf->rfield = self;
    return self;
}

#define GET_SF() FrtSortField *sf = (FrtSortField *)DATA_PTR(self)

/*
 *  call-seq:
 *     sort_field.reverse? -> bool
 *
 *  Return true if the field is to be reverse sorted. This attribute is set
 *  when you create the sort_field.
 */
static VALUE frb_sf_is_reverse(VALUE self) {
    GET_SF();
    return sf->reverse ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     sort_field.name -> symbol
 *
 *  Returns the name of the field to be sorted.
 */
static VALUE frb_sf_get_name(VALUE self) {
    GET_SF();
    return sf->field ? ID2SYM(sf->field) : Qnil;
}

/*
 *  call-seq:
 *     sort_field.type -> symbol
 *
 *  Return the type of sort. Should be one of; +:auto+, +:integer+, +:float+,
 *  +:string+, +:byte+, +:doc_id+ or +:score+.
 */
static VALUE frb_sf_get_type(VALUE self) {
    GET_SF();
    switch (sf->type) {
        case FRT_SORT_TYPE_BYTE:    return sym_byte;
        case FRT_SORT_TYPE_INTEGER: return sym_integer;
        case FRT_SORT_TYPE_FLOAT:   return sym_float;
        case FRT_SORT_TYPE_STRING:  return sym_string;
        case FRT_SORT_TYPE_AUTO:    return sym_auto;
        case FRT_SORT_TYPE_DOC:     return sym_doc_id;
        case FRT_SORT_TYPE_SCORE:   return sym_score;
    }
    return Qnil;
}

/*
 *  call-seq:
 *     sort_field.comparator -> symbol
 *
 *  TODO: currently unsupported
 */
static VALUE frb_sf_get_comparator(VALUE self) {
    return Qnil;
}

/*
 *  call-seq:
 *     sort_field.to_s -> string
 *
 *  Return a human readable string describing this +sort_field+.
 */
static VALUE frb_sf_to_s(VALUE self) {
    GET_SF();
    char *str = frt_sort_field_to_s(sf);
    VALUE rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

/****************************************************************************
 *
 * Sort Methods
 *
 ****************************************************************************/

static size_t frb_sort_size(const void *p) {
    return sizeof(FrtSort);
    (void)p;
}

static void frb_sort_free(void *p) {
    frt_sort_destroy((FrtSort *)p);
}

static void frb_sort_mark(void *p) {
    FrtSort *sort = (FrtSort *)p;
    int i;
    for (i = 0; i < sort->size; i++) {
        if (sort->sort_fields[i]->rfield)
            rb_gc_mark(sort->sort_fields[i]->rfield);
    }
}

const rb_data_type_t frb_sort_t = {
    .wrap_struct_name = "FrbSort",
    .function = {
        .dmark = frb_sort_mark,
        .dfree = frb_sort_free,
        .dsize = frb_sort_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_sort_alloc(VALUE klass) {
    VALUE self;
    FrtSort *sort = frt_sort_new();
    sort->destroy_all = false;
    self = TypedData_Wrap_Struct(klass, &frb_sort_t, sort);
    return self;
}

static void frb_parse_sort_str(FrtSort *sort, char *xsort_str) {
    FrtSortField *sf;
    char *comma, *end, *e, *s;
    const int len = strlen(xsort_str);
    char *sort_str = FRT_ALLOC_N(char, len + 2);
    strcpy(sort_str, xsort_str);

    end = &sort_str[len];

    s = sort_str;

    while ((s < end)
           && (NULL != (comma = strchr(s, ',')) || (NULL != (comma = end)))) {
        bool reverse = false;
        /* strip spaces */
        e = comma;
        while ((isspace(*s) || *s == ':') && s < e) s++;
        while (isspace(e[-1]) && s < e) e--;
        *e = '\0';
        if (e > (s + 4) && strcmp("DESC", &e[-4]) == 0) {
            reverse = true;
            e -= 4;
            while (isspace(e[-1]) && s < e) e--;
        }
        *e = '\0';

        if (strcmp("SCORE", s) == 0) {
            sf = frt_sort_field_score_new(reverse);
        } else if (strcmp("DOC_ID", s) == 0) {
            sf = frt_sort_field_doc_new(reverse);
        } else {
            sf = frt_sort_field_auto_new(rb_intern(s), reverse);
        }
        frb_get_sf(sf);
        frt_sort_add_sort_field(sort, sf);
        s = comma + 1;
    }
    free(sort_str);
}

static void frb_sort_add(FrtSort *sort, VALUE rsf, bool reverse) {
    FrtSortField *sf;
    switch (TYPE(rsf)) {
        case T_DATA:
            TypedData_Get_Struct(rsf, FrtSortField, &frb_sort_field_t, sf);
            if (reverse) sf->reverse = !sf->reverse;
            frt_sort_add_sort_field(sort, sf);
            break;
        case T_SYMBOL:
            sf = frt_sort_field_auto_new(frb_field(rsf), reverse);
            /* need to give it a ruby object so it'll be freed when the
             * sort is garbage collected */
            rsf = frb_get_sf(sf);
            frt_sort_add_sort_field(sort, sf);
            break;
        case T_STRING:
            frb_parse_sort_str(sort, rs2s(rsf));
            break;
        default:
            rb_raise(rb_eArgError, "Unknown SortField Type");
            break;
    }
}

#define GET_SORT() FrtSort *sort = (FrtSort *)DATA_PTR(self)
/*
 *  call-seq:
 *     Sort.new(sort_fields = [SortField::SCORE, SortField::DOC_ID], reverse = false) -> Sort
 *
 *  Create a new Sort object. If +reverse+ is true, all sort_fields will be
 *  reversed so if any of them are already reversed the  will be turned back
 *  to their natural order again. By default
 */
static VALUE frb_sort_init(int argc, VALUE *argv, VALUE self) {
    int i;
    VALUE rfields, rreverse;
    bool reverse = false;
    bool has_sfd = false;
    GET_SORT();
    switch (rb_scan_args(argc, argv, "02", &rfields, &rreverse)) {
        case 2: reverse = RTEST(rreverse);
        case 1:
                if (TYPE(rfields) == T_ARRAY) {
                    int i;
                    for (i = 0; i < RARRAY_LEN(rfields); i++) {
                        frb_sort_add(sort, RARRAY_PTR(rfields)[i], reverse);
                    }
                } else {
                    frb_sort_add(sort, rfields, reverse);
                }
                for (i = 0; i < sort->size; i++) {
                    if (sort->sort_fields[i] == FRT_SORT_FIELD_DOC) has_sfd = true;
                }
                if (!has_sfd) {
                    frt_sort_add_sort_field(sort, FRT_SORT_FIELD_DOC);
                }
                break;
        case 0:
                frt_sort_add_sort_field(sort, FRT_SORT_FIELD_SCORE);
                frt_sort_add_sort_field(sort, FRT_SORT_FIELD_DOC);
    }

    return self;
}

/*
 *  call-seq:
 *     sort.fields -> Array
 *
 *  Returns an array of the SortFields held by the Sort object.
 */
static VALUE frb_sort_get_fields(VALUE self) {
    GET_SORT();
    VALUE rfields = rb_ary_new2(sort->size);
    int i;
    for (i = 0; i < sort->size; i++) {
        rb_ary_store(rfields, i, sort->sort_fields[i]->rfield);
    }
    return rfields;
}

/*
 *  call-seq:
 *     sort.to_s -> string
 *
 *  Returns a human readable string representing the sort object.
 */
static VALUE frb_sort_to_s(VALUE self) {
    GET_SORT();
    char *str = frt_sort_to_s(sort);
    VALUE rstr = rb_str_new2(str);
    free(str);
    return rstr;
}

/****************************************************************************
 *
 * Searcher Methods
 *
 ****************************************************************************/

static void frb_sea_free(void *p) {
    FrtSearcher *sea = (FrtSearcher *)p;
    sea->close(sea);
}

#define GET_SEA() FrtSearcher *sea = (FrtSearcher *)DATA_PTR(self)

/*
 *  call-seq:
 *     searcher.close -> nil
 *
 *  Close the searcher. The garbage collector will do this for you or you can
 *  call this method explicitly.
 */
static VALUE frb_sea_close(VALUE self) {
    GET_SEA();
    ((struct RData *)(self))->data = NULL;
    ((struct RData *)(self))->dmark = NULL;
    ((struct RData *)(self))->dfree = NULL;
    sea->close(sea);
    return Qnil;
}

/*
 *  call-seq:
 *     searcher.reader -> IndexReader
 *
 *  Return the IndexReader wrapped by this searcher.
 */
static VALUE frb_sea_get_reader(VALUE self) {
    GET_SEA();
    return ((FrtIndexSearcher *)sea)->ir->rir;
}

/*
 *  call-seq:
 *     searcher.doc_freq(field, term) -> integer
 *
 *  Return the number of documents in which the term +term+ appears in the
 *  field +field+.
 */
static VALUE frb_sea_doc_freq(VALUE self, VALUE rfield, VALUE rterm) {
    GET_SEA();
    return INT2FIX(sea->doc_freq(sea, frb_field(rfield), StringValuePtr(rterm)));
}

/*
 *  call-seq:
 *     searcher.get_document(doc_id) -> LazyDoc
 *     searcher[doc_id] -> LazyDoc
 *
 *  Retrieve a document from the index. See LazyDoc for more details on the
 *  document returned. Documents are referenced internally by document ids
 *  which are returned by the Searchers search methods.
 */
static VALUE frb_sea_doc(VALUE self, VALUE rdoc_id) {
    GET_SEA();
    return frb_get_lazy_doc(sea->get_lazy_doc(sea, FIX2INT(rdoc_id)));
}

/*
 *  call-seq:
 *     searcher.max_doc -> number
 *
 *  Returns 1 + the maximum document id in the index. It is the
 *  document_id that will be used by the next document added to the index. If
 *  there are no deletions, this number also refers to the number of documents
 *  in the index.
 */
static VALUE frb_sea_max_doc(VALUE self) {
    GET_SEA();
    return INT2FIX(sea->max_doc(sea));
}

static float call_filter_proc(int doc_id, float score, FrtSearcher *sea, void *arg) {
    VALUE val = rb_funcall((VALUE)arg, id_call, 3, INT2FIX(doc_id), rb_float_new((double)score), sea->rsea);
    switch (TYPE(val)) {
        case T_NIL:
        case T_FALSE:
            return 0.0f;
        case T_FLOAT:
        {
            double d = NUM2DBL(val);
            return (d >= 0.0 && d <= 1.0) ? (float)d : 1.0f;
        }
        default:
            return 1.0f;
    }
}

typedef struct CWrappedFilter {
    FrtFilter super;
    VALUE  rfilter;
} CWrappedFilter;

#define CWF(filt) ((CWrappedFilter *)(filt))

static unsigned long long cwfilt_hash(FrtFilter *filt) {
    return (unsigned long long)NUM2ULONG(rb_funcall(CWF(filt)->rfilter, id_hash, 0));
}

static int cwfilt_eq(FrtFilter *filt, FrtFilter *o) {
    return RTEST(rb_funcall(CWF(filt)->rfilter, id_eql, 1, CWF(o)->rfilter));
}

static FrtBitVector *cwfilt_get_bv_i(FrtFilter *filt, FrtIndexReader *ir) {
    VALUE rbv = rb_funcall(CWF(filt)->rfilter, id_bits, 1, ir->rir);
    FrtBitVector *bv;
    bv = DATA_PTR(rbv);
    FRT_REF(bv);
    return bv;
}

FrtFilter *frb_get_cwrapped_filter(VALUE rval) {
    FrtFilter *filter;
    if (frb_is_cclass(rval) && DATA_PTR(rval)) {
        filter = DATA_PTR(rval);
        FRT_REF(filter);
    } else {
        filter               = filt_new(CWrappedFilter);
        filter->hash         = &cwfilt_hash;
        filter->eq           = &cwfilt_eq;
        filter->get_bv_i     = &cwfilt_get_bv_i;
        CWF(filter)->rfilter = rval;
    }
    return filter;
}

static FrtTopDocs *frb_sea_search_internal(FrtQuery *query, VALUE roptions, FrtSearcher *sea) {
    VALUE rval;
    int offset = 0, limit = 10;
    FrtFilter *filter = NULL;
    FrtSort *sort = NULL;
    FrtTopDocs *td;

    FrtPostFilter post_filter_holder;
    FrtPostFilter *post_filter = NULL;

    int ex_code = 0;
    const char *msg = NULL;

    if (Qnil != roptions) {
        if (Qnil != (rval = rb_hash_aref(roptions, sym_offset))) {
            offset = FIX2INT(rval);
            if (offset < 0)
                rb_raise(rb_eArgError, ":offset must be >= 0");
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_limit))) {
            if (TYPE(rval) == T_FIXNUM) {
                limit = FIX2INT(rval);
                if (limit <= 0) {
                    rb_raise(rb_eArgError, ":limit must be > 0");
                }
            } else if (rval == sym_all) {
                limit = INT_MAX;
            } else {
                rb_raise(rb_eArgError, "%s is not a sensible :limit value "
                         "Please use a positive integer or :all",
                         rs2s(rb_obj_as_string(rval)));
            }
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_filter))) {
            filter = frb_get_cwrapped_filter(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_c_filter_proc))) {
                post_filter = DATA_PTR(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_filter_proc))) {
            if (rb_respond_to(rval, id_call)) {
                if (post_filter) {
                    rb_raise(rb_eArgError, "Cannot pass both :filter_proc and "
                             ":c_filter_proc to the same search");
                }
                post_filter_holder.filter_func = &call_filter_proc;
                post_filter_holder.arg = (void *)rval;
                post_filter = &post_filter_holder;
            }
            else {
                post_filter = DATA_PTR(rval);
            }
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_sort))) {
            if (TYPE(rval) != T_DATA || CLASS_OF(rval) == cSortField) {
                rval = frb_sort_init(1, &rval, frb_sort_alloc(cSort));
            }
            TypedData_Get_Struct(rval, FrtSort, &frb_sort_t, sort);
        }
    }

    FRT_TRY
        td = sea->search(sea, query, offset, limit, filter, sort, post_filter, 0);
        if (filter) frt_filt_deref(filter);
    FRT_XCATCHALL
        ex_code = xcontext.excode;
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY

    if (ex_code && msg) { frb_raise(ex_code, msg); }

    return td;
}

/*
 *  call-seq:
 *     searcher.search(query, options = {}) -> TopDocs
 *
 *  Run a query through the Searcher on the index. A TopDocs object is
 *  returned with the relevant results. The +query+ is a built in Query
 *  object. Here are the options;
 *
 *  === Options
 *
 *  :offset::       Default: 0. The offset of the start of the section of the
 *                  result-set to return. This is used for paging through
 *                  results. Let's say you have a page size of 10. If you
 *                  don't find the result you want among the first 10 results
 *                  then set +:offset+ to 10 and look at the next 10 results,
 *                  then 20 and so on.
 *  :limit::        Default: 10. This is the number of results you want
 *                  returned, also called the page size. Set +:limit+ to
 *                  +:all+ to return all results
 *  :sort::         A Sort object or sort string describing how the field
 *                  should be sorted. A sort string is made up of field names
 *                  which cannot contain spaces and the word "DESC" if you
 *                  want the field reversed, all separated by commas. For
 *                  example; "rating DESC, author, title". Note that Ferret
 *                  will try to determine a field's type by looking at the
 *                  first term in the index and seeing if it can be parsed as
 *                  an integer or a float. Keep this in mind as you may need
 *                  to specify a fields type to sort it correctly. For more
 *                  on this, see the documentation for SortField
 *  :filter::       a Filter object to filter the search results with
 *  :filter_proc::  a filter Proc is a Proc which takes the doc_id, the score
 *                  and the Searcher object as its parameters and returns
 *                  either a Boolean value specifying whether the result
 *                  should be included in the result set, or a Float between 0
 *                  and 1.0 to be used as a factor to scale the score of the
 *                  object. This can be used, for example, to weight the score
 *                  of a matched document by it's age.
 */
static VALUE frb_sea_search(int argc, VALUE *argv, VALUE self) {
    GET_SEA();
    VALUE rquery, roptions;
    rb_scan_args(argc, argv, "11", &rquery, &roptions);
    FrtQuery *query = DATA_PTR(rquery);
    FrtTopDocs *td = frb_sea_search_internal(query, roptions, sea);
    return frb_get_td(td, self);
}

/*
 *  call-seq:
 *     searcher.search_each(query, options = {}) {|doc_id, score| do_something}
 *         -> total_hits
 *
 *  Run a query through the Searcher on the index. A TopDocs object is
 *  returned with the relevant results. The +query+ is a Query object. The
 *  Searcher#search_each method yields the internal document id (used to
 *  reference documents in the Searcher object like this; +searcher[doc_id]+)
 *  and the search score for that document. It is possible for the score to be
 *  greater than 1.0 for some queries and taking boosts into account. This
 *  method will also normalize scores to the range 0.0..1.0 when the max-score
 *  is greater than 1.0. Here are the options;
 *
 *  === Options
 *
 *  :offset::       Default: 0. The offset of the start of the section of the
 *                  result-set to return. This is used for paging through
 *                  results. Let's say you have a page size of 10. If you
 *                  don't find the result you want among the first 10 results
 *                  then set +:offset+ to 10 and look at the next 10 results,
 *                  then 20 and so on.
 *  :limit::        Default: 10. This is the number of results you want
 *                  returned, also called the page size. Set +:limit+ to
 *                  +:all+ to return all results
 *  :sort::         A Sort object or sort string describing how the field
 *                  should be sorted. A sort string is made up of field names
 *                  which cannot contain spaces and the word "DESC" if you
 *                  want the field reversed, all separated by commas. For
 *                  example; "rating DESC, author, title". Note that Ferret
 *                  will try to determine a field's type by looking at the
 *                  first term in the index and seeing if it can be parsed as
 *                  an integer or a float. Keep this in mind as you may need
 *                  to specify a fields type to sort it correctly. For more
 *                  on this, see the documentation for SortField
 *  :filter::       a Filter object to filter the search results with
 *  :filter_proc::  a filter Proc is a Proc which takes the doc_id, the score
 *                  and the Searcher object as its parameters and returns a
 *                  Boolean value specifying whether the result should be
 *                  included in the result set.
 */
static VALUE frb_sea_search_each(int argc, VALUE *argv, VALUE self) {
    int i;
    float max_score;
    FrtTopDocs *td;
    VALUE rquery, roptions, rtotal_hits;
    GET_SEA();

    rb_scan_args(argc, argv, "11", &rquery, &roptions);
    FrtQuery *q = DATA_PTR(rquery);

    td = frb_sea_search_internal(q, roptions, sea);

    max_score = (td->max_score > 1.0f) ? td->max_score : 1.0f;

    /* yield normalized scores */
    for (i = 0; i < td->size; i++) {
        rb_yield_values(2, INT2FIX(td->hits[i]->doc), rb_float_new((double)(td->hits[i]->score/max_score)));
    }

    rtotal_hits = INT2FIX(td->total_hits);
    frt_td_destroy(td);

    return rtotal_hits;
}

/*
 *  call-seq:
 *     searcher.scan(query, options = {}) -> Array (doc_nums)
 *
 *  Run a query through the Searcher on the index, ignoring scoring and
 *  starting at +:start_doc+ and stopping when +:limit+ matches have been
 *  found. It returns an array of the matching document numbers.
 *
 *  There is a big performance advange when using this search method on a very
 *  large index when there are potentially thousands of matching documents and
 *  you only want say 50 of them. The other search methods need to look at
 *  every single match to decide which one has the highest score. This search
 *  method just needs to find +:limit+ number of matches before it returns.
 *
 *  === Options
 *
 *  :start_doc::    Default: 0. The start document to start the search from.
 *                  NOTE very carefully that this is not the same as the
 *                  +:offset+ parameter used in the other search methods which
 *                  refers to the offset in the result-set. This is the
 *                  document to start the scan from. So if you scanning
 *                  through the index in increments of 50 documents at a time
 *                  you need to use the last matched doc in the previous
 *                  search to start your next search. See the example below.
 *  :limit::        Default: 50. This is the number of results you want
 *                  returned, also called the page size. Set +:limit+ to
 *                  +:all+ to return all results.
 *  TODO: add option to return loaded documents instead
 *
 *  === Options
 *
 *    start_doc = 0
 *    begin
 *      results = @searcher.scan(query, :start_doc => start_doc)
 *      yield results # or do something with them
 *      start_doc = results.last
 *      # start_doc will be nil now if results is empty, ie no more matches
 *    end while start_doc
 */
static VALUE frb_sea_scan(int argc, VALUE *argv, VALUE self) {
    int i, count;
    VALUE rval, rquery, roptions;
    int *doc_array;
    VALUE rdoc_array;
    int start_doc = 0, limit = 50;
    GET_SEA();
    rb_scan_args(argc, argv, "11", &rquery, &roptions);
    FrtQuery *q = DATA_PTR(rquery);

    if (Qnil != roptions) {
        Check_Type(roptions, T_HASH);
        if (Qnil != (rval = rb_hash_aref(roptions, sym_start_doc))) {
            Check_Type(rval, T_FIXNUM);
            start_doc = FIX2INT(rval);
            if (start_doc < 0) {
                rb_raise(rb_eArgError, ":start_doc must be >= 0");
            }
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_limit))) {
            if (TYPE(rval) == T_FIXNUM) {
                limit = FIX2INT(rval);
                if (limit <= 0) {
                    rb_raise(rb_eArgError, ":limit must be > 0");
                }
            }
            else if (rval == sym_all) {
                limit = INT_MAX;
            }
            else {
                rb_raise(rb_eArgError, "%s is not a sensible :limit value "
                         "Please use a positive integer or :all",
                         rs2s(rb_obj_as_string(rval)));
            }
        }
    }

    doc_array = FRT_ALLOC_N(int, limit);
    count = sea->search_unscored(sea, q, doc_array, limit, start_doc);
    rdoc_array = rb_ary_new2(count);
    for (i = 0; i < count; i++) {
      rb_ary_store(rdoc_array, i, INT2FIX(doc_array[i]));
    }
    free(doc_array);
    return rdoc_array;
}

/*
 *  call-seq:
 *     searcher.explain(query, doc_id) -> Explanation
 *
 *  Create an explanation object to explain the score returned for a
 *  particular document at +doc_id+ in the index for the query +query+.
 *
 *  Usually used like this;
 *
 *    puts searcher.explain(query, doc_id).to_s
 */

static size_t frb_explanation_size(const void *p) {
    return sizeof(FrtExplanation);
    (void)p;
}

static void frb_explanation_free(void *p) {
    frt_expl_destroy((FrtExplanation *)p);
}

const rb_data_type_t frb_explanation_t = {
    .wrap_struct_name = "FrbExplanation",
    .function = {
        .dmark = NULL,
        .dfree = frb_explanation_free,
        .dsize = frb_explanation_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_expl_alloc(VALUE rclass) {
    FrtExplanation *e = FRT_ALLOC(FrtExplanation);
    return TypedData_Wrap_Struct(rclass, &frb_explanation_t, e);
}

static VALUE frb_sea_explain(VALUE self, VALUE rquery, VALUE rdoc_id) {
    GET_SEA();
    FrtQuery *query = DATA_PTR(rquery);
    FrtExplanation *expl;
    expl = sea->explain(sea, query, FIX2INT(rdoc_id));
    return TypedData_Wrap_Struct(cExplanation, &frb_explanation_t, expl);
}

/*
 *  call-seq:
 *     searcher.highlight(query, doc_id, field, options = {}) -> Array
 *
 *  Returns an array of strings with the matches highlighted.
 *
 *  === Options
 *
 *  :excerpt_length::   Default: 150. Length of excerpt to show. Highlighted
 *                      terms will be in the centre of the excerpt. Set to
 *                      :all to highlight the entire field.
 *  :num_excerpts::     Default: 2. Number of excerpts to return.
 *  :pre_tag::          Default: "<b>". Tag to place to the left of the match.
 *                      You'll probably want to change this to a "<span>" tag
 *                      with a class. Try "\033[7m" for use in a terminal.
 *  :post_tag::         Default: "</b>". This tag should close the +:pre_tag+.
 *                      Try tag "\033[m" in the terminal.
 *  :ellipsis::         Default: "...". This is the string that is appended at
 *                      the beginning and end of excerpts (unless the excerpt
 *                      hits the start or end of the field. You'll probably
 *                      want to change this so a Unicode ellipsis character.
 */
static VALUE frb_sea_highlight(int argc, VALUE *argv, VALUE self) {
    GET_SEA();
    VALUE rquery, rdoc_id, rfield, roptions, v;
    int excerpt_length = 150;
    int num_excerpts = 2;
    const char *pre_tag = "<b>";
    const char *post_tag = "</b>";
    const char *ellipsis = "...";
    char **excerpts;

    rb_scan_args(argc, argv, "31", &rquery, &rdoc_id, &rfield, &roptions);
    FrtQuery *query = DATA_PTR(rquery);
    if (argc > 3) {
        if (TYPE(roptions) != T_HASH) {
           rb_raise(rb_eArgError, "The fourth argument to Searcher#highlight must be a hash");
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_num_excerpts))) {
            num_excerpts =  FIX2INT(v);
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_excerpt_length))) {
            if (v == sym_all) {
                num_excerpts = 1;
                excerpt_length = INT_MAX/2;
            }
            else {
                excerpt_length = FIX2INT(v);
            }
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_pre_tag))) {
            pre_tag = rs2s(rb_obj_as_string(v));
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_post_tag))) {
            post_tag = rs2s(rb_obj_as_string(v));
        }
        if (Qnil != (v = rb_hash_aref(roptions, sym_ellipsis))) {
            ellipsis = rs2s(rb_obj_as_string(v));
        }
    }

    if ((excerpts = frt_searcher_highlight(sea,
                                       query,
                                       FIX2INT(rdoc_id),
                                       frb_field(rfield),
                                       excerpt_length,
                                       num_excerpts,
                                       pre_tag,
                                       post_tag,
                                       ellipsis)) != NULL) {
        const int size = frt_ary_size(excerpts);
        int i;
        VALUE rexcerpts = rb_ary_new2(size);

        for (i = 0; i < size; i++) {
          rb_ary_store(rexcerpts, i, rb_str_new2(excerpts[i]));
        }
        frt_ary_destroy(excerpts, &free);
        return rexcerpts;
    }
    return Qnil;
}

/****************************************************************************
 * Searcher Methods
 ****************************************************************************/

static size_t frb_index_searcher_size(const void *p) {
    return sizeof(FrtIndexSearcher);
    (void)p;
}

static void frb_sea_mark(void *p) {
    FrtIndexSearcher *isea = (FrtIndexSearcher *)p;
    if (isea->ir) {
        if (isea->ir->rir)
            rb_gc_mark(isea->ir->rir);
        if (isea->ir->store && isea->ir->store->rstore)
            rb_gc_mark(isea->ir->store->rstore);
    }
}

const rb_data_type_t frb_index_searcher_t = {
    .wrap_struct_name = "FrbIndexSearcher",
    .function = {
        .dmark = frb_sea_mark,
        .dfree = frb_sea_free,
        .dsize = frb_index_searcher_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_sea_alloc(VALUE rclass) {
    FrtSearcher *sea = frt_isea_alloc();
    ((FrtIndexSearcher *)sea)->ir = NULL;
    return TypedData_Wrap_Struct(rclass, &frb_index_searcher_t, sea);
}

/*
 *  call-seq:
 *     Searcher.new(obj) -> Searcher
 *
 *  Create a new Searcher object. +dir+ can either be a string path to an
 *  index directory on the file-system, an actual Ferret::Store::Directory
 *  object or a Ferret::Index::IndexReader. You should use the IndexReader for
 *  searching multiple indexes. Just open the IndexReader on multiple
 *  directories.
 */
static VALUE frb_sea_init(VALUE self, VALUE obj) {
    FrtStore *store = NULL;
    FrtIndexReader *ir = NULL;
    FrtSearcher *sea;
    if (TYPE(obj) == T_STRING) {
        frb_create_dir(obj);
        store = frt_open_fs_store(rs2s(obj));
        ir = frt_ir_open(NULL, store);
        ir->rir = TypedData_Wrap_Struct(cIndexReader, &frb_index_reader_t, ir);
    } else {
        // Check_Type(obj, T_DATA);
        if (rb_obj_is_kind_of(obj, cDirectory) == Qtrue) {
            store = DATA_PTR(obj);
            ir = frt_ir_open(NULL, store);
            ir->rir = TypedData_Wrap_Struct(cIndexReader, &frb_index_reader_t, ir);
        } else if (rb_obj_is_kind_of(obj, cIndexReader) == Qtrue) {
            TypedData_Get_Struct(obj, FrtIndexReader, &frb_index_reader_t, ir);
        } else {
            rb_raise(rb_eArgError, "Unknown type for argument to IndexSearcher.new");
        }
    }
    TypedData_Get_Struct(self, FrtSearcher, &frb_index_searcher_t, sea);
    frt_isea_init(sea, ir);
    sea->rsea = self;
    return self;
}

/****************************************************************************
 *
 * MultiSearcher Methods
 *
 ****************************************************************************/

static size_t frb_multi_searcher_size(const void *p) {
    return sizeof(FrtMultiSearcher);
    (void)p;
}

static void frb_ms_free(void *p) {
    FrtSearcher *sea = (FrtSearcher *)p;
    FrtMultiSearcher *msea = (FrtMultiSearcher *)sea;
    free(msea->searchers);
    frt_searcher_close(sea);
}

static void frb_ms_mark(void *p) {
    int i;
    FrtMultiSearcher *msea = (FrtMultiSearcher *)p;
    for (i = 0; i < msea->s_cnt; i++) {
        if (msea->searchers[i]->rsea)
            rb_gc_mark(msea->searchers[i]->rsea);
    }
}

const rb_data_type_t frb_multi_searcher_t = {
    .wrap_struct_name = "FrbMultiSearcher",
    .function = {
        .dmark = frb_ms_mark,
        .dfree = frb_ms_free,
        .dsize = frb_multi_searcher_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_ms_alloc(VALUE rclass) {
    FrtSearcher *s = frt_msea_alloc();
    ((FrtMultiSearcher *)s)->s_cnt = 0;
    return TypedData_Wrap_Struct(rclass, &frb_multi_searcher_t, s);
}

/*
 *  call-seq:
 *     MultiSearcher.new(searcher*) -> searcher
 *
 *  Create a new MultiSearcher by passing a list of subsearchers to the
 *  constructor.
 */
static VALUE frb_ms_init(int argc, VALUE *argv, VALUE self) {
    int i, j, top = 0, capa = argc;

    VALUE rsearcher;
    FrtSearcher **searchers = FRT_ALLOC_N(FrtSearcher *, capa);
    FrtSearcher *sea;
    for (i = 0; i < argc; i++) {
        rsearcher = argv[i];
        switch (TYPE(rsearcher)) {
            case T_ARRAY:
                capa += RARRAY_LEN(rsearcher);
                FRT_REALLOC_N(searchers, FrtSearcher *, capa);
                for (j = 0; j < RARRAY_LEN(rsearcher); j++) {
                    VALUE rs = RARRAY_PTR(rsearcher)[j];
                    sea = DATA_PTR(rs);
                    searchers[top++] = sea;
                }
                break;
            case T_DATA:
                sea = DATA_PTR(rsearcher);
                searchers[top++] = sea;
                break;
            default:
                rb_raise(rb_eArgError, "Can't add class %s to MultiSearcher", rb_obj_classname(rsearcher));
                break;
        }
    }
    TypedData_Get_Struct(self, FrtSearcher, &frb_multi_searcher_t, sea);
    frt_msea_init(sea, searchers, top);
    sea->rsea = self;
    return self;
}

VALUE frb_get_q(FrtQuery *q) {
    VALUE self = q->rquery;

    if (self == 0 || self == Qnil) {
        switch (q->type) {
            case TERM_QUERY:
                self = TypedData_Wrap_Struct(cTermQuery, &frb_term_query_t, q);
                break;
            case MULTI_TERM_QUERY:
                self = TypedData_Wrap_Struct(cMultiTermQuery, &frb_multi_term_query_t, q);
                break;
            case BOOLEAN_QUERY:
                self = TypedData_Wrap_Struct(cBooleanQuery, &frb_boolean_query_t, q);
                break;
            case PHRASE_QUERY:
                self = TypedData_Wrap_Struct(cPhraseQuery, &frb_phrase_query_t, q);
                break;
            case CONSTANT_QUERY:
                self = TypedData_Wrap_Struct(cConstantScoreQuery, &frb_constant_score_query_t, q);
                break;
            case FILTERED_QUERY:
                self = TypedData_Wrap_Struct(cFilteredQuery, &frb_filtered_query_t, q);
                break;
            case MATCH_ALL_QUERY:
                self = TypedData_Wrap_Struct(cMatchAllQuery, &frb_match_all_query_t, q);
                break;
            case RANGE_QUERY:
                self = TypedData_Wrap_Struct(cRangeQuery, &frb_range_query_t, q);
                break;
            case TYPED_RANGE_QUERY:
                self = TypedData_Wrap_Struct(cTypedRangeQuery, &frb_typed_range_query_t, q);
                break;
            case WILD_CARD_QUERY:
                self = TypedData_Wrap_Struct(cWildcardQuery, &frb_wilcard_query_t, q);
                break;
            case FUZZY_QUERY:
                self = TypedData_Wrap_Struct(cFuzzyQuery, &frb_fuzzy_query_t, q);
                break;
            case PREFIX_QUERY:
                self = TypedData_Wrap_Struct(cPrefixQuery, &frb_prefix_query_t, q);
                break;
            case SPAN_TERM_QUERY:
                self = TypedData_Wrap_Struct(cSpanMultiTermQuery, &frb_span_multi_term_query_t, q);
                break;
            case SPAN_MULTI_TERM_QUERY:
                self = TypedData_Wrap_Struct(cSpanPrefixQuery, &frb_span_prefix_query_t, q);
                break;
            case SPAN_PREFIX_QUERY:
                self = TypedData_Wrap_Struct(cSpanTermQuery, &frb_span_term_query_t, q);
                break;
            case SPAN_FIRST_QUERY:
                self = TypedData_Wrap_Struct(cSpanFirstQuery, &frb_span_first_query_t, q);
                break;
            case SPAN_OR_QUERY:
                self = TypedData_Wrap_Struct(cSpanOrQuery, &frb_span_or_query_t, q);
                break;
            case SPAN_NOT_QUERY:
                self = TypedData_Wrap_Struct(cSpanNotQuery, &frb_span_not_query_t, q);
                break;
            case SPAN_NEAR_QUERY:
                self = TypedData_Wrap_Struct(cSpanNearQuery, &frb_span_near_query_t, q);
                break;
            default:
                rb_raise(rb_eArgError, "Unknown query type");
                break;
        }
        q->rquery = self;
    }
    return self;
}

/****************************************************************************
 *
 * Init Function
 *
 ****************************************************************************/

/*
 *  Document-class: Ferret::Search::Hit
 *
 *  == Summary
 *
 *  A hit represents a single document match for a search. It holds the
 *  document id of the document that matches along with the score for the
 *  match. The score is a positive Float value. The score contained in a hit
 *  is not normalized so it can be greater than 1.0. To normalize scores to
 *  the range 0.0..1.0 divide the scores by TopDocs#max_score.
 */
static void Init_Hit(void) {
    const char *hit_class = "Hit";
    cHit = rb_struct_define(hit_class, "doc", "score", NULL);
    rb_set_class_path(cHit, mSearch, hit_class);
    rb_const_set(mSearch, rb_intern(hit_class), cHit);
    id_doc = rb_intern("doc");
    id_score = rb_intern("score");
}

/*
 *  Document-class: Ferret::Search::TopDocs
 *
 *  == Summary
 *
 *  A TopDocs object holds a result set for a search. The number of documents
 *  that matched the query his held in TopDocs#total_hits. The actual
 *  results are in the Array TopDocs#hits. The number of hits returned is
 *  limited by the +:limit+ option so the size of the +hits+ array will not
 *  always be equal to the value of +total_hits+. Finally TopDocs#max_score
 *  holds the maximum score of any match (not necessarily the maximum score
 *  contained in the +hits+ array) so it can be used to normalize scores. For
 *  example, to print doc ids with scores out of 100.0 you could do this;
 *
 *    top_docs.hits.each do |hit|
 *      puts "#{hit.doc} scored #{hit.score * 100.0 / top_docs.max_score}"
 *    end
 */
static void Init_TopDocs(void) {
    const char *td_class = "TopDocs";
    cTopDocs = rb_struct_define(td_class,
                                "total_hits",
                                "hits",
                                "max_score",
                                "searcher",
                                NULL);
    rb_set_class_path(cTopDocs, mSearch, td_class);
    rb_const_set(mSearch, rb_intern(td_class), cTopDocs);
    rb_define_method(cTopDocs, "to_s", frb_td_to_s, -1);
    rb_define_method(cTopDocs, "to_json", frb_td_to_json, 0);
    id_hits = rb_intern("hits");
    id_total_hits = rb_intern("total_hits");
    id_max_score = rb_intern("max_score");
    id_searcher = rb_intern("searcher");
}

/*
 *  Document-class: Ferret::Search::Explanation
 *
 *  == Summary
 *
 *  Explanation is used to give a description of why a document matched with
 *  the score that it did. Use the Explanation#to_s or Explanation#to_html
 *  methods to display the explanation in a human readable format. Creating
 *  explanations is an expensive operation so it should only be used for
 *  debugging purposes. To create an explanation use the Searcher#explain
 *  method.
 *
 *  == Example
 *
 *    puts searcher.explain(query, doc_id).to_s
 */
static void Init_Explanation(void) {
    cExplanation = rb_define_class_under(mSearch, "Explanation", rb_cObject);
    rb_define_alloc_func(cExplanation, frb_expl_alloc);

    rb_define_method(cExplanation, "to_s", frb_expl_to_s, 0);
    rb_define_method(cExplanation, "to_html", frb_expl_to_html, 0);
    rb_define_method(cExplanation, "score", frb_expl_score, 0);
}

/*
 *  Document-class: Ferret::Search::Query
 *
 *  == Summary
 *
 *  Abstract class representing a query to the index. There are a number of
 *  concrete Query implementations;
 *
 *  * TermQuery
 *  * MultiTermQuery
 *  * BooleanQuery
 *  * PhraseQuery
 *  * ConstantScoreQuery
 *  * FilteredQuery
 *  * MatchAllQuery
 *  * RangeQuery
 *  * WildcardQuery
 *  * FuzzyQuery
 *  * PrefixQuery
 *  * Spans::SpanTermQuery
 *  * Spans::SpanFirstQuery
 *  * Spans::SpanOrQuery
 *  * Spans::SpanNotQuery
 *  * Spans::SpanNearQuery
 *
 *  Explore these classes for the query right for you. The queries are passed
 *  to the Searcher#search* methods.
 *
 *  === Query Boosts
 *
 *  Queries have a boost value so that you can make the results of one query
 *  more important than the results of another query when combining them in a
 *  BooleanQuery. For example, documents on Rails. To avoid getting results
 *  for train rails you might also add the tern Ruby but Rails is the more
 *  important term so you'd give it a boost.
 */
static void Init_Query(void) {
    cQuery = rb_define_class_under(mSearch, "Query", rb_cObject);

    rb_define_method(cQuery, "to_s", frb_q_to_s, -1);
    rb_define_method(cQuery, "boost", frb_q_get_boost, 0);
    rb_define_method(cQuery, "boost=", frb_q_set_boost, 1);
    rb_define_method(cQuery, "eql?", frb_q_eql, 1);
    rb_define_method(cQuery, "==", frb_q_eql, 1);
    rb_define_method(cQuery, "hash", frb_q_hash, 0);
    rb_define_method(cQuery, "terms", frb_q_get_terms, 1);
}

/*
 *  Document-class: Ferret::Search::TermQuery
 *
 *  == Summary
 *
 *  TermQuery is the most basic query and it is the building block for most
 *  other queries. It basically matches documents that contain a specific term
 *  in a specific field.
 *
 *  == Example
 *
 *    query = TermQuery.new(:content, "rails")
 *
 *    # untokenized fields can also be searched with this query;
 *    query = TermQuery.new(:title, "Shawshank Redemption")
 *
 *  Notice the all lowercase term Rails. This is important as most analyzers will
 *  downcase all text added to the index. The title in this case was not
 *  tokenized so the case would have been left as is.
 */
static void Init_TermQuery(void) {
    cTermQuery = rb_define_class_under(mSearch, "TermQuery", cQuery);
    rb_define_alloc_func(cTermQuery, frb_tq_alloc);
    rb_define_method(cTermQuery, "initialize", frb_tq_init, 2);
}

/*
 *  Document-class: Ferret::Search::MultiTermQuery
 *
 *  == Summary
 *
 *  MultiTermQuery matches documents that contain one of a list of terms in a
 *  specific field. This is the basic building block for queries such as;
 *
 *  * PrefixQuery
 *  * WildcardQuery
 *  * FuzzyQuery
 *
 *  MultiTermQuery is very similar to a boolean "Or" query. It is highly
 *  optimized though as it focuses on a single field.
 *
 *  == Example
 *
 *    multi_term_query = MultiTermQuery.new(:content, :max_term => 10)
 *
 *    multi_term_query << "Ruby" << "Ferret" << "Rails" << "Search"
 */
static void Init_MultiTermQuery(void) {
    id_default_max_terms = rb_intern("@@default_max_terms");
    sym_max_terms = ID2SYM(rb_intern("max_terms"));
    sym_min_score = ID2SYM(rb_intern("min_score"));

    cMultiTermQuery = rb_define_class_under(mSearch, "MultiTermQuery", cQuery);
    rb_define_alloc_func(cMultiTermQuery, frb_mtq_alloc);
    rb_cvar_set(cMultiTermQuery, id_default_max_terms, INT2FIX(512));
    rb_define_singleton_method(cMultiTermQuery, "default_max_terms", frb_mtq_get_dmt, 0);
    rb_define_singleton_method(cMultiTermQuery, "default_max_terms=", frb_mtq_set_dmt, 1);

    rb_define_method(cMultiTermQuery, "initialize", frb_mtq_init, -1);
    rb_define_method(cMultiTermQuery, "add_term", frb_mtq_add_term, -1);
    rb_define_method(cMultiTermQuery, "<<", frb_mtq_add_term, -1);
}

static void Init_BooleanClause(void);

/*
 *  Document-class: Ferret::Search::BooleanQuery
 *
 *  == Summary
 *
 *  A BooleanQuery is used for combining many queries into one. This is best
 *  illustrated with an example.
 *
 *  == Example
 *
 *  Lets say we wanted to find all documents with the term "Ruby" in the
 *  +:title+ and the term "Ferret" in the +:content+ field or the +:title+
 *  field written before January 2006. You could build the query like this.
 *
 *    tq1 = TermQuery.new(:title, "ruby")
 *    tq21 = TermQuery.new(:title, "ferret")
 *    tq22 = TermQuery.new(:content, "ferret")
 *    bq2 = BooleanQuery.new
 *    bq2 << tq21 << tq22
 *
 *    rq3 = RangeQuery.new(:written, :< => "200601")
 *
 *    query = BooleanQuery.new
 *    query.add_query(tq1, :must).add_query(bq2, :must).add_query(rq3, :must)
 */
static void Init_BooleanQuery(void) {
    cBooleanQuery = rb_define_class_under(mSearch, "BooleanQuery", cQuery);
    rb_define_alloc_func(cBooleanQuery, frb_bq_alloc);

    rb_define_method(cBooleanQuery, "initialize", frb_bq_init, -1);
    rb_define_method(cBooleanQuery, "add_query", frb_bq_add_query, -1);
    rb_define_method(cBooleanQuery, "<<", frb_bq_add_query, -1);

    Init_BooleanClause();
}

/*
 *  Document-class: Ferret::Search::BooleanQuery::BooleanClause
 *
 *  == Summary
 *
 *  A BooleanClause holes a single query within a BooleanQuery specifying
 *  wither the query +:must+ match, +:should+ match or +:must_not+ match.
 *  BooleanClauses can be used to pass a clause from one BooleanQuery to
 *  another although it is generally easier just to add a query directly to a
 *  BooleanQuery using the BooleanQuery#add_query method.
 *
 *  == Example
 *
 *    clause1 = BooleanClause.new(query1, :should)
 *    clause2 = BooleanClause.new(query2, :should)
 *
 *    query = BooleanQuery.new
 *    query << clause1 << clause2
 */
static void Init_BooleanClause(void) {
    sym_should = ID2SYM(rb_intern("should"));
    sym_must = ID2SYM(rb_intern("must"));
    sym_must_not = ID2SYM(rb_intern("must_not"));

    cBooleanClause = rb_define_class_under(cBooleanQuery, "BooleanClause", rb_cObject);
    rb_define_alloc_func(cBooleanClause, frb_bc_alloc);

    rb_define_method(cBooleanClause, "initialize", frb_bc_init, -1);
    rb_define_method(cBooleanClause, "query", frb_bc_get_query, 0);
    rb_define_method(cBooleanClause, "query=", frb_bc_set_query, 1);
    rb_define_method(cBooleanClause, "required?", frb_bc_is_required, 0);
    rb_define_method(cBooleanClause, "prohibited?", frb_bc_is_prohibited, 0);
    rb_define_method(cBooleanClause, "occur=", frb_bc_set_occur, 1);
    rb_define_method(cBooleanClause, "to_s", frb_bc_to_s, 0);
}

/*
 *  Document-class: Ferret::Search::RangeQuery
 *
 *  == Summary
 *
 *  RangeQuery is used to find documents with terms in a range.
 *  RangeQuerys are usually used on untokenized fields like date fields or
 *  number fields.
 *
 *  == Example
 *
 *  To find all documents written between January 1st 2006 and January 26th
 *  2006 inclusive you would write the query like this;
 *
 *    query = RangeQuery.new(:create_date, :>= "20060101", :<= "20060126")
 *
 *  == Range queries on numbers
 *
 *  There is now a new query called TypedRangeQuery which detects the type of
 *  the range and if the range is numerical it will find a numerical range.
 *  This allows you to do range queries with negative numbers and without
 *  having to pad the field. However, RangeQuery will perform a lot faster on
 *  large indexes so if you are working with a very large index you will need
 *  to normalize your number fields so that they are a fixed width and always
 *  positive. That way the standard String range query will do fine.
 *
 *  For example, if you have the numbers;
 *
 *    [10, -999, -90, 100, 534]
 *
 *  Then the can be normalized to;
 *
 *    # note that we have added 1000 to all numbers to make them all positive
 *    [1010, 0001, 0910, 1100, 1534]
 *
 */
static void Init_RangeQuery(void) {
    sym_upper = ID2SYM(rb_intern("upper"));
    sym_lower = ID2SYM(rb_intern("lower"));
    sym_upper_exclusive = ID2SYM(rb_intern("upper_exclusive"));
    sym_lower_exclusive = ID2SYM(rb_intern("lower_exclusive"));
    sym_include_upper = ID2SYM(rb_intern("include_upper"));
    sym_include_lower = ID2SYM(rb_intern("include_lower"));

    sym_less_than = ID2SYM(rb_intern("<"));
    sym_less_than_or_equal_to = ID2SYM(rb_intern("<="));
    sym_greater_than = ID2SYM(rb_intern(">"));
    sym_greater_than_or_equal_to = ID2SYM(rb_intern(">="));

    cRangeQuery = rb_define_class_under(mSearch, "RangeQuery", cQuery);
    rb_define_alloc_func(cRangeQuery, frb_rq_alloc);

    rb_define_method(cRangeQuery, "initialize", frb_rq_init, 2);
}

/*
 *  Document-class: Ferret::Search::TypedRangeQuery
 *
 *  == Summary
 *
 *  TypedRangeQuery is used to find documents with terms in a range.
 *  RangeQuerys are usually used on untokenized fields like date fields or
 *  number fields. TypedRangeQuery is particularly useful for fields with
 *  unnormalized numbers, both positive and negative, integer and float.
 *
 *  == Example
 *
 *  To find all documents written between January 1st 2006 and January 26th
 *  2006 inclusive you would write the query like this;
 *
 *    query = RangeQuery.new(:create_date, :>= "-1.0", :<= "10.0")
 *
 *  == Performance Note
 *
 *  TypedRangeQuery works by converting all the terms in a field to numbers
 *  and then comparing those numbers with the range bondaries. This can have
 *  quite an impact on performance on large indexes so in those cases it is
 *  usually better to use a standard RangeQuery. This will require a little
 *  work on your behalf. See RangeQuery for notes on how to do this.
 */
static void Init_TypedRangeQuery(void) {
    cTypedRangeQuery = rb_define_class_under(mSearch, "TypedRangeQuery", cQuery);
    rb_define_alloc_func(cTypedRangeQuery, frb_trq_alloc);

    rb_define_method(cTypedRangeQuery, "initialize", frb_trq_init, 2);
}

/*
 *  Document-class: Ferret::Search::PhraseQuery
 *
 *  == Summary
 *
 *  PhraseQuery matches phrases like "the quick brown fox". Most people are
 *  familiar with phrase queries having used them in most internet search
 *  engines.
 *
 *  === Slop
 *
 *  Ferret's phrase queries a slightly more advanced. You can match phrases
 *  with a slop, ie the match isn't exact but it is good enough. The slop is
 *  basically the word edit distance of the phrase. For example, "the quick
 *  brown fox" with a slop of 1 would match "the quick little brown fox". With
 *  a slop of 2 it would match "the brown quick fox".
 *
 *    query = PhraseQuery.new(:content)
 *    query << "the" << "quick" << "brown" << "fox"
 *
 *    # matches => "the quick brown fox"
 *
 *    query.slop = 1
 *    # matches => "the quick little brown fox"
 *                               |__1__^
 *
 *    query.slop = 2
 *    # matches => "the brown quick _____ fox"
 *                        ^_____2_____|
 *
 *  == Multi-PhraseQuery
 *
 *  Phrase queries can also have multiple terms in a single position. Let's
 *  say for example that we want to match synonyms for quick like "fast" and
 *  "speedy". You could the query like this;
 *
 *    query = PhraseQuery.new(:content)
 *    query << "the" << ["quick", "fast", "speed"] << ["brown", "red"] << "fox"
 *    # matches => "the quick red fox"
 *    # matches => "the fast brown fox"
 *
 *    query.slop = 1
 *    # matches => "the speedy little red fox"
 *
 *  You can also leave positions blank. Lets say you wanted to match "the
 *  quick <> fox" where "<>" could match anything (but not nothing). You'd
 *  build this query like this;
 *
 *    query = PhraseQuery.new(:content)
 *    query.add_term("the").add_term("quick").add_term("fox", 2)
 *    # matches => "the quick yellow fox"
 *    # matches => "the quick alkgdhaskghaskjdh fox"
 *
 *  The second parameter to PhraseQuery#add_term is the position increment for
 *  the term. It is one by default meaning that every time you add a term it
 *  is expected to follow the previous term. But setting it to 2 or greater
 *  you are leaving empty spaces in the term.
 *
 *  There are also so tricks you can do by setting the position increment to
 *  0. With a little help from your analyzer you can actually tag bold or
 *  italic text for example. If you want more information about this, ask on
 *  the mailing list.
 */
static void Init_PhraseQuery(void) {
    cPhraseQuery = rb_define_class_under(mSearch, "PhraseQuery", cQuery);
    rb_define_alloc_func(cPhraseQuery, frb_phq_alloc);

    rb_define_method(cPhraseQuery, "initialize", frb_phq_init, -1);
    rb_define_method(cPhraseQuery, "add_term", frb_phq_add, -1);
    rb_define_method(cPhraseQuery, "<<", frb_phq_add, -1);
    rb_define_method(cPhraseQuery, "slop", frb_phq_get_slop, 0);
    rb_define_method(cPhraseQuery, "slop=", frb_phq_set_slop, 1);
}

/*
 *  Document-class: Ferret::Search::PrefixQuery
 *
 *  == Summary
 *
 *  A prefix query is like a TermQuery except that it matches any term with a
 *  specific prefix. PrefixQuery is expanded into a MultiTermQuery when
 *  submitted in a search.
 *
 *  == Example
 *
 *  PrefixQuery is very useful for matching a tree structure category
 *  hierarchy. For example, let's say you have the categories;
 *
 *    "cat1/"
 *    "cat1/sub_cat1"
 *    "cat1/sub_cat2"
 *    "cat2"
 *    "cat2/sub_cat1"
 *    "cat2/sub_cat2"
 *
 *  Lets say you want to match everything in category 2. You'd build the query
 *  like this;
 *
 *    query = PrefixQuery.new(:category, "cat2")
 *    # matches => "cat2"
 *    # matches => "cat2/sub_cat1"
 *    # matches => "cat2/sub_cat2"
 */
static void Init_PrefixQuery(void) {
    cPrefixQuery = rb_define_class_under(mSearch, "PrefixQuery", cQuery);
    rb_define_alloc_func(cPrefixQuery, frb_prq_alloc);

    rb_define_method(cPrefixQuery, "initialize", frb_prq_init, -1);
}

/*
 *  Document-class: Ferret::Search::WildcardQuery
 *
 *  == Summary
 *
 *  WildcardQuery is a simple pattern matching query. There are two wild-card
 *  characters.
 *
 *  * "*" which matches 0 or more characters
 *  * "?" which matches a single character
 *
 *  == Example
 *
 *    query = WildcardQuery.new(:field, "h*og")
 *    # matches => "hog"
 *    # matches => "hot dog"
 *
 *    query = WildcardQuery.new(:field, "fe?t")
 *    # matches => "feat"
 *    # matches => "feet"
 *
 *    query = WildcardQuery.new(:field, "f?ll*")
 *    # matches => "fill"
 *    # matches => "falling"
 *    # matches => "folly"
 */
static void Init_WildcardQuery(void) {
    cWildcardQuery = rb_define_class_under(mSearch, "WildcardQuery", cQuery);
    rb_define_alloc_func(cWildcardQuery, frb_wcq_alloc);

    rb_define_method(cWildcardQuery, "initialize", frb_wcq_init, -1);
}

/*
 *  Document-class: Ferret::Search::FuzzyQuery
 *
 *  == Summary
 *
 *  FuzzyQuery uses the Levenshtein distance formula for measuring the
 *  similarity between two terms. For example, weak and week have one letter
 *  difference and they are four characters long so the simlarity is 75% or
 *  0.75. You can use this query to match terms that are very close to the
 *  search term.
 *
 *  == Example
 *
 *  FuzzyQuery can be quite useful for find documents that wouldn't normally
 *  be found because of typos.
 *
 *    FuzzyQuery.new(:field, "google",
 *                   :min_similarity => 0.6,
 *                   :prefix_length => 2)
 *    # matches => "gogle", "goggle", "googol", "googel"
 */
static void Init_FuzzyQuery(void) {
    id_default_min_similarity = rb_intern("@@default_min_similarity");
    id_default_prefix_length = rb_intern("@@default_prefix_length");

    sym_min_similarity = ID2SYM(rb_intern("min_similarity"));
    sym_prefix_length = ID2SYM(rb_intern("prefix_length"));

    cFuzzyQuery = rb_define_class_under(mSearch, "FuzzyQuery", cQuery);
    rb_define_alloc_func(cFuzzyQuery, frb_fq_alloc);
    rb_cvar_set(cFuzzyQuery, id_default_min_similarity, rb_float_new(0.5));
    rb_cvar_set(cFuzzyQuery, id_default_prefix_length, INT2FIX(0));

    rb_define_singleton_method(cFuzzyQuery, "default_min_similarity", frb_fq_get_dms, 0);
    rb_define_singleton_method(cFuzzyQuery, "default_min_similarity=", frb_fq_set_dms, 1);
    rb_define_singleton_method(cFuzzyQuery, "default_prefix_length", frb_fq_get_dpl, 0);
    rb_define_singleton_method(cFuzzyQuery, "default_prefix_length=", frb_fq_set_dpl, 1);

    rb_define_method(cFuzzyQuery, "initialize",     frb_fq_init, -1);
    rb_define_method(cFuzzyQuery, "prefix_length",  frb_fq_pre_len, 0);
    rb_define_method(cFuzzyQuery, "min_similarity", frb_fq_min_sim, 0);
}

/*
 *  Document-class: Ferret::Search::MatchAllQuery
 *
 *  == Summary
 *
 *  MatchAllQuery matches all documents in the index. You might want use this
 *  query in combination with a filter, however, ConstantScoreQuery is
 *  probably better in that circumstance.
 */
static void Init_MatchAllQuery(void) {
    cMatchAllQuery = rb_define_class_under(mSearch, "MatchAllQuery", cQuery);
    rb_define_alloc_func(cMatchAllQuery, frb_maq_alloc);

    rb_define_method(cMatchAllQuery, "initialize", frb_maq_init, 0);
}

/*
 *  Document-class: Ferret::Search::ConstantScoreQuery
 *
 *  == Summary
 *
 *  ConstantScoreQuery is a way to turn a Filter into a Query. It matches all
 *  documents that its filter matches with a constant score. This is a very
 *  fast query, particularly when run more than once (since filters are
 *  cached). It is also used internally be RangeQuery.
 *
 *  == Example
 *
 *  Let's say for example that you often need to display all documents created
 *  on or after June 1st. You could create a ConstantScoreQuery like this;
 *
 *    query = ConstantScoreQuery.new(RangeFilter.new(:created_on, :>= => "200606"))
 *
 *  Once this is run once the results are cached and will be returned very
 *  quickly in future requests.
 */
static void Init_ConstantScoreQuery(void) {
    cConstantScoreQuery = rb_define_class_under(mSearch, "ConstantScoreQuery", cQuery);
    rb_define_alloc_func(cConstantScoreQuery, frb_csq_alloc);

    rb_define_method(cConstantScoreQuery, "initialize", frb_csq_init, 1);
}

/*
 *  Document-class: Ferret::Search::FilteredQuery
 *
 *  == Summary
 *
 *  FilteredQuery offers you a way to apply a filter to a specific query.
 *  The FilteredQuery would then by added to a BooleanQuery to be combined
 *  with other queries. There is not much point in passing a FilteredQuery
 *  directly to a Searcher#search method unless you are applying more than one
 *  filter since the search method also takes a filter as a parameter.
 */
static void Init_FilteredQuery(void) {
    cFilteredQuery = rb_define_class_under(mSearch, "FilteredQuery", cQuery);
    rb_define_alloc_func(cFilteredQuery, frb_fqq_alloc);

    rb_define_method(cFilteredQuery, "initialize", frb_fqq_init, 2);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanTermQuery
 *
 *  == Summary
 *
 *  A SpanTermQuery is the Spans version of TermQuery, the only difference
 *  being that it returns the start and end offset of all of its matches for
 *  use by enclosing SpanQueries.
 */
static void Init_SpanTermQuery(void) {
    cSpanTermQuery = rb_define_class_under(mSpans, "SpanTermQuery", cQuery);
    rb_define_alloc_func(cSpanTermQuery, frb_spantq_alloc);

    rb_define_method(cSpanTermQuery, "initialize", frb_spantq_init, 2);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanMultiTermQuery
 *
 *  == Summary
 *
 *  A SpanMultiTermQuery is the Spans version of MultiTermQuery, the only
 *  difference being that it returns the start and end offset of all of its
 *  matches for use by enclosing SpanQueries.
 */
static void Init_SpanMultiTermQuery(void) {
    cSpanMultiTermQuery = rb_define_class_under(mSpans, "SpanMultiTermQuery", cQuery);
    rb_define_alloc_func(cSpanMultiTermQuery, frb_spanmtq_alloc);

    rb_define_method(cSpanMultiTermQuery, "initialize", frb_spanmtq_init, 2);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanPrefixQuery
 *
 *  == Summary
 *
 *  A SpanPrefixQuery is the Spans version of PrefixQuery, the only difference
 *  being that it returns the start and end offset of all of its matches for
 *  use by enclosing SpanQueries.
 */
static void Init_SpanPrefixQuery(void) {
    cSpanPrefixQuery = rb_define_class_under(mSpans, "SpanPrefixQuery", cQuery);
    rb_define_alloc_func(cSpanPrefixQuery, frb_spanprq_alloc);

    rb_define_method(cSpanPrefixQuery, "initialize", frb_spanprq_init, -1);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanFirstQuery
 *
 *  == Summary
 *
 *  A SpanFirstQuery restricts a query to search in the first +end+ bytes of a
 *  field. This is useful since often the most important information in a
 *  document is at the start of the document.
 *
 *  == Example
 *
 *  To find all documents where "ferret" is within the first 100 characters
 *  (really bytes);
 *
 *    query = SpanFirstQuery.new(SpanTermQuery.new(:content, "ferret"), 100)
 *
 *  == NOTE
 *
 *  SpanFirstQuery only works with other SpanQueries.
 */
static void Init_SpanFirstQuery(void) {
    cSpanFirstQuery = rb_define_class_under(mSpans, "SpanFirstQuery", cQuery);
    rb_define_alloc_func(cSpanFirstQuery, frb_spanfq_alloc);

    rb_define_method(cSpanFirstQuery, "initialize", frb_spanfq_init, 2);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanNearQuery
 *
 *  == Summary
 *
 *  A SpanNearQuery is like a combination between a PhraseQuery and a
 *  BooleanQuery. It matches sub-SpanQueries which are added as clauses but
 *  those clauses must occur within a +slop+ edit distance of each other. You
 *  can also specify that clauses must occur +in_order+.
 *
 *  == Example
 *
 *    query = SpanNearQuery.new(:slop => 2)
 *    query << SpanTermQuery.new(:field, "quick")
 *    query << SpanTermQuery.new(:field, "brown")
 *    query << SpanTermQuery.new(:field, "fox")
 *    # matches => "quick brown speckled sleepy fox"
 *                                 |______2______^
 *    # matches => "quick brown speckled fox"
 *                                  |__1__^
 *    # matches => "brown quick _____ fox"
 *                    ^_____2_____|
 *
 *    query = SpanNearQuery.new(:slop => 2, :in_order => true)
 *    query << SpanTermQuery.new(:field, "quick")
 *    query << SpanTermQuery.new(:field, "brown")
 *    query << SpanTermQuery.new(:field, "fox")
 *    # matches => "quick brown speckled sleepy fox"
 *                                 |______2______^
 *    # matches => "quick brown speckled fox"
 *                                  |__1__^
 *    # doesn't match => "brown quick _____ fox"
 *    #  not in order       ^_____2_____|
 *
 *  == NOTE
 *
 *  SpanNearQuery only works with other SpanQueries.
 */
static void Init_SpanNearQuery(void) {
    sym_slop = ID2SYM(rb_intern("slop"));
    sym_in_order = ID2SYM(rb_intern("in_order"));
    sym_clauses = ID2SYM(rb_intern("clauses"));

    cSpanNearQuery = rb_define_class_under(mSpans, "SpanNearQuery", cQuery);
    rb_define_alloc_func(cSpanNearQuery, frb_spannq_alloc);

    rb_define_method(cSpanNearQuery, "initialize", frb_spannq_init, -1);
    rb_define_method(cSpanNearQuery, "add", frb_spannq_add, 1);
    rb_define_method(cSpanNearQuery, "<<", frb_spannq_add, 1);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanOrQuery
 *
 *  == Summary
 *
 *  SpanOrQuery is just like a BooleanQuery with all +:should+ clauses.
 *  However, the difference is that all sub-clauses must be SpanQueries and
 *  the resulting query can then be used within other SpanQueries like
 *  SpanNearQuery.
 *
 *  == Example
 *
 *  Combined with SpanNearQuery we can create a multi-PhraseQuery like query;
 *
 *    quick_query = SpanOrQuery.new()
 *    quick_query << SpanTermQuery.new(:field, "quick")
 *    quick_query << SpanTermQuery.new(:field, "fast")
 *    quick_query << SpanTermQuery.new(:field, "speedy")
 *
 *    colour_query = SpanOrQuery.new()
 *    colour_query << SpanTermQuery.new(:field, "red")
 *    colour_query << SpanTermQuery.new(:field, "brown")
 *
 *
 *    query = SpanNearQuery.new(:slop => 2, :in_order => true)
 *    query << quick_query
 *    query << colour_query
 *    query << SpanTermQuery.new(:field, "fox")
 *    # matches => "quick red speckled sleepy fox"
 *                              |______2______^
 *    # matches => "speedy brown speckled fox"
 *                                  |__1__^
 *    # doesn't match => "brown fast _____ fox"
 *    #  not in order       ^_____2____|
 *
 *  == NOTE
 *
 *  SpanOrQuery only works with other SpanQueries.
 */
static void Init_SpanOrQuery(void) {
    cSpanOrQuery = rb_define_class_under(mSpans, "SpanOrQuery", cQuery);
    rb_define_alloc_func(cSpanOrQuery, frb_spanoq_alloc);

    rb_define_method(cSpanOrQuery, "initialize", frb_spanoq_init, -1);
    rb_define_method(cSpanOrQuery, "add", frb_spanoq_add, 1);
    rb_define_method(cSpanOrQuery, "<<", frb_spanoq_add, 1);
}

/*
 *  Document-class: Ferret::Search::Spans::SpanNotQuery
 *
 *  == Summary
 *
 *  SpanNotQuery is like a BooleanQuery with a +:must_not+ clause. The
 *  difference being that the resulting query can be used in another
 *  SpanQuery.
 *
 *  == Example
 *
 *  Let's say you wanted to search for all documents with the term "rails"
 *  near the start but without the term "train" near the start. This would
 *  allow the term "train" to occur later on in the document.
 *
 *    rails_query = SpanFirstQuery.new(FrtSpanTermQuery.new(:content, "rails"), 100)
 *    train_query = SpanFirstQuery.new(FrtSpanTermQuery.new(:content, "train"), 100)
 *    query = SpanNotQuery.new(rails_query, train_query)
 *
 *  == NOTE
 *
 *  SpanOrQuery only works with other SpanQueries.
 */
static void Init_SpanNotQuery(void) {
    cSpanNotQuery = rb_define_class_under(mSpans, "SpanNotQuery", cQuery);
    rb_define_alloc_func(cSpanNotQuery, frb_spanxq_alloc);

    rb_define_method(cSpanNotQuery, "initialize", frb_spanxq_init, 2);
}

/* rdoc hack
extern VALUE mFerret = rb_define_module("Ferret");
extern VALUE mSearch = rb_define_module_under(mFerret, "Search");
*/

/*
 *  Document-module: Ferret::Search::Spans
 *
 *  == Summary
 *
 *  The Spans module contains a number of SpanQueries. SpanQueries, unlike
 *  regular queries, also return the start and end offsets of all of their
 *  matches so they can be used to limit queries to a certain position in the
 *  field. They are often used in combination to perform special types of
 *  PhraseQuery.
 */
static void Init_Spans(void) {
    mSpans = rb_define_module_under(mSearch, "Spans");
    Init_SpanTermQuery();
    Init_SpanMultiTermQuery();
    Init_SpanPrefixQuery();
    Init_SpanFirstQuery();
    Init_SpanNearQuery();
    Init_SpanOrQuery();
    Init_SpanNotQuery();
}

/*
 *  Document-class: Ferret::Search::RangeFilter
 *
 *  == Summary
 *
 *  RangeFilter filters a set of documents which contain a lexicographical
 *  range of terms (ie "aaa", "aab", "aac", etc). See also RangeQuery
 *
 *  == Example
 *
 *  Find all documents created before 5th of September 2002.
 *
 *    filter = RangeFilter.new(:created_on, :< => "20020905")
 *
 *  == Number fields
 *
 *  See RangeQuery for notes on how to use the RangeFilter on a field
 *  containing numbers.
 */
static void Init_RangeFilter(void) {
    cRangeFilter = rb_define_class_under(mSearch, "RangeFilter", cFilter);
    frb_mark_cclass(cRangeFilter);
    rb_define_alloc_func(cRangeFilter, frb_rf_alloc);

    rb_define_method(cRangeFilter, "initialize", frb_rf_init, 2);
}

/*
 *  Document-class: Ferret::Search::TypedRangeFilter
 *
 *  == Summary
 *
 *  TypedRangeFilter filters a set of documents which contain a
 *  lexicographical range of terms (ie "aaa", "aab", "aac", etc), unless the
 *  range boundaries happen to be numbers (positive, negative, integer,
 *  float), in which case a numerical filter is applied. See also
 *  TypedRangeQuery
 *
 *  == Example
 *
 *  Find all products that cost less than  or equal to $50.00.
 *
 *    filter = TypedRangeFilter.new(:created_on, :<= => "50.00")
 */
static void Init_TypedRangeFilter(void) {
    cTypedRangeFilter = rb_define_class_under(mSearch, "TypedRangeFilter", cFilter);
    frb_mark_cclass(cTypedRangeFilter);
    rb_define_alloc_func(cTypedRangeFilter, frb_trf_alloc);

    rb_define_method(cTypedRangeFilter, "initialize", frb_trf_init, 2);
}

/*
 *  Document-class: Ferret::Search::QueryFilter
 *
 *  == Summary
 *
 *  QueryFilter can be used to restrict one queries results by another queries
 *  results, basically "and"ing them together. Of course you could easily use
 *  a BooleanQuery to do this. The reason you may choose to use a QueryFilter
 *  is that Filter results are cached so if you have one query that is often
 *  added to other queries you may want to use a QueryFilter for performance
 *  reasons.
 *
 *  == Example
 *
 *  Let's say you have a field +:approved+ which you set to yes when a
 *  document is approved for display. You'll probably want to add a Filter
 *  which filters approved documents to display to your users. This is the
 *  perfect use case for a QueryFilter.
 *
 *    filter = QueryFilter.new(TermQuery.new(:approved, "yes"))
 *
 *  Just remember to use the same QueryFilter each time to take advantage of
 *  caching. Don't create a new one for each request. Of course, this won't
 *  work in a CGI application.
 */
static void Init_QueryFilter(void) {
    cQueryFilter = rb_define_class_under(mSearch, "QueryFilter", cFilter);
    frb_mark_cclass(cQueryFilter);
    rb_define_alloc_func(cQueryFilter, frb_qf_alloc);

    rb_define_method(cQueryFilter, "initialize", frb_qf_init, 1);
}

/*
 *  Document-class: Ferret::Search::Filter
 *
 *  == Summary
 *
 *  A Filter is used to filter query results. It is usually passed to one of
 *  Searcher's search methods however it can also be used inside a
 *  ConstantScoreQuery or a FilteredQuery. To implement your own Filter you
 *  must implement the method #get_bitvector(index_reader) which returns a
 *  BitVector with set bits corresponding to documents that are allowed by
 *  this Filter.
 *
 *  TODO add support for user implemented Filter.
 *  TODO add example of user implemented Filter.
 */
static void Init_Filter(void) {
    id_bits = rb_intern("bits");
    cFilter = rb_define_class_under(mSearch, "Filter", rb_cObject);
    frb_mark_cclass(cFilter);
    rb_define_alloc_func(cConstantScoreQuery, frb_csq_alloc);

    rb_define_method(cFilter, "bits", frb_f_get_bits, 1);
    rb_define_method(cFilter, "to_s", frb_f_to_s, 0);
}

/*
 *  Document-class: Ferret::Search::SortField
 *
 *  == Summary
 *
 *  A SortField is used to sort the result-set of a search be the contents of
 *  a field. The following types of sort_field are available;
 *
 *  * :auto
 *  * :integer
 *  * :float
 *  * :string
 *  * :byte
 *  * :doc_id
 *  * :score
 *
 *  The type of the SortField is set by passing it as a parameter to the
 *  constructor. The +:auto+ type specifies that the SortField should detect
 *  the sort type by looking at the data in the field. This is the default
 *  :type value although it is recommended that you explicitly specify the
 *  fields type.
 *
 *  == Example
 *
 *    title_sf = SortField.new(:title, :type => :string)
 *    rating_sf = SortField.new(:rating, :type => float, :reverse => true)
 *
 *
 *  Note 1: Care should be taken when using the :auto sort-type since numbers
 *  will occur before other strings in the index so if you are sorting a field
 *  with both numbers and strings (like a title field which might have "24"
 *  and "Prison Break") then the sort_field will think it is sorting integers
 *  when it really should be sorting strings.
 *
 *  Note 2: When sorting by integer, integers are only 4 bytes so anything
 *  larger will cause strange sorting behaviour.
 */

static void Init_SortField(void) {
    /* option hash keys for SortField#initialize */
    sym_type = ID2SYM(rb_intern("type"));
    sym_reverse = ID2SYM(rb_intern("reverse"));
    sym_comparator = ID2SYM(rb_intern("comparator"));

    /* Sort types */
    sym_integer = ID2SYM(rb_intern("integer"));
    sym_float = ID2SYM(rb_intern("float"));
    sym_string = ID2SYM(rb_intern("string"));
    sym_auto = ID2SYM(rb_intern("auto"));
    sym_doc_id = ID2SYM(rb_intern("doc_id"));
    sym_score = ID2SYM(rb_intern("score"));
    sym_byte = ID2SYM(rb_intern("byte"));

    cSortField = rb_define_class_under(mSearch, "SortField", rb_cObject);
    rb_define_alloc_func(cSortField, frb_sf_alloc);

    rb_define_method(cSortField, "initialize", frb_sf_init, -1);
    rb_define_method(cSortField, "reverse?", frb_sf_is_reverse, 0);
    rb_define_method(cSortField, "name", frb_sf_get_name, 0);
    rb_define_method(cSortField, "type", frb_sf_get_type, 0);
    rb_define_method(cSortField, "comparator", frb_sf_get_comparator, 0);
    rb_define_method(cSortField, "to_s", frb_sf_to_s, 0);

    rb_define_const(cSortField, "SCORE", TypedData_Wrap_Struct(cSortField, &frb_sort_field_t, FRT_SORT_FIELD_SCORE));
    FRT_SORT_FIELD_SCORE->rfield = rb_const_get(cSortField, rb_intern("SCORE"));

    rb_define_const(cSortField, "SCORE_REV", TypedData_Wrap_Struct(cSortField, &frb_sort_field_t, FRT_SORT_FIELD_SCORE_REV));
    FRT_SORT_FIELD_SCORE_REV->rfield = rb_const_get(cSortField, rb_intern("SCORE_REV"));

    rb_define_const(cSortField, "DOC_ID", TypedData_Wrap_Struct(cSortField, &frb_sort_field_t, FRT_SORT_FIELD_DOC));
    oSORT_FIELD_DOC = rb_const_get(cSortField, rb_intern("DOC_ID"));
    FRT_SORT_FIELD_DOC->rfield = oSORT_FIELD_DOC;

    rb_define_const(cSortField, "DOC_ID_REV", TypedData_Wrap_Struct(cSortField, &frb_sort_field_t, FRT_SORT_FIELD_DOC_REV));
    FRT_SORT_FIELD_DOC_REV->rfield = rb_const_get(cSortField, rb_intern("DOC_ID_REV"));
}

/*
 *  Document-class: Ferret::Search::Sort
 *
 *  == Summary
 *
 *  A Sort object is used to combine and apply a list of SortFields. The
 *  SortFields are applied in the order they are added to the SortObject.
 *
 *  == Example
 *
 *  Here is how you would create a Sort object that sorts first by rating and
 *  then  by title;
 *
 *    sf_rating = SortField.new(:rating, :type => :float, :reverse => true)
 *    sf_title = SortField.new(:title, :type => :string)
 *    sort = Sort.new([sf_rating, sf_title])
 *
 *  Remember that the :type parameter for SortField is set to :auto be default
 *  be I strongly recommend you specify a :type value.
 */
static void Init_Sort(void) {
    /* Sort */
    cSort = rb_define_class_under(mSearch, "Sort", rb_cObject);
    rb_define_alloc_func(cSort, frb_sort_alloc);

    rb_define_method(cSort, "initialize", frb_sort_init, -1);
    rb_define_method(cSort, "fields", frb_sort_get_fields, 0);
    rb_define_method(cSort, "to_s", frb_sort_to_s, 0);

    rb_define_const(cSort, "RELEVANCE", frb_sort_init(0, NULL, frb_sort_alloc(cSort)));
    rb_define_const(cSort, "INDEX_ORDER", frb_sort_init(1, &oSORT_FIELD_DOC, frb_sort_alloc(cSort)));
}

/*
 *  Document-class: Ferret::Search::Searcher
 *
 *  == Summary
 *
 *  The Searcher class basically performs the task that Ferret was built for.
 *  It searches the index. To search the index the Searcher class wraps an
 *  IndexReader so many of the tasks that you can perform on an IndexReader
 *  are also available on a searcher including, most importantly, accessing
 *  stored documents.
 *
 *  The main methods that you need to know about when using a Searcher are the
 *  search methods. There is the Searcher#search_each method which iterates
 *  through the results by document id and score and there is the
 *  Searcher#search method which returns a TopDocs object. Another important
 *  difference to note is that the Searcher#search_each method normalizes the
 *  score to a value in the range 0.0..1.0 if the max_score is greater than
 *  1.0. Searcher#search does not. Apart from that they take the same
 *  parameters and work the same way.
 *
 *  == Example
 *
 *    searcher = Searcher.new("/path/to/index")
 *
 *    searcher.search_each(TermQuery.new(:content, "ferret")
 *                         :filter => RangeFilter.new(:date, :< => "2006"),
 *                         :sort => "date DESC, title") do |doc_id, score|
 *        puts "#{searcher[doc_id][title] scored #{score}"
 *    end
 */
static void Init_Searcher(void) {
    /* option hash keys for Searcher#search */
    sym_offset          = ID2SYM(rb_intern("offset"));
    sym_limit           = ID2SYM(rb_intern("limit"));
    sym_start_doc       = ID2SYM(rb_intern("start_doc"));
    sym_all             = ID2SYM(rb_intern("all"));
    sym_filter          = ID2SYM(rb_intern("filter"));
    sym_filter_proc     = ID2SYM(rb_intern("filter_proc"));
    sym_c_filter_proc   = ID2SYM(rb_intern("c_filter_proc"));
    sym_sort            = ID2SYM(rb_intern("sort"));

    sym_excerpt_length  = ID2SYM(rb_intern("excerpt_length"));
    sym_num_excerpts    = ID2SYM(rb_intern("num_excerpts"));
    sym_pre_tag         = ID2SYM(rb_intern("pre_tag"));
    sym_post_tag        = ID2SYM(rb_intern("post_tag"));
    sym_ellipsis        = ID2SYM(rb_intern("ellipsis"));

    /* Searcher */
    cSearcher = rb_define_class_under(mSearch, "Searcher", rb_cObject);
    rb_define_alloc_func(cSearcher, frb_sea_alloc);

    rb_define_method(cSearcher, "initialize", frb_sea_init, 1);
    rb_define_method(cSearcher, "close", frb_sea_close, 0);
    rb_define_method(cSearcher, "reader", frb_sea_get_reader, 0);
    rb_define_method(cSearcher, "doc_freq", frb_sea_doc_freq, 2);
    rb_define_method(cSearcher, "get_document", frb_sea_doc, 1);
    rb_define_method(cSearcher, "[]", frb_sea_doc, 1);
    rb_define_method(cSearcher, "max_doc", frb_sea_max_doc, 0);
    rb_define_method(cSearcher, "search", frb_sea_search, -1);
    rb_define_method(cSearcher, "search_each", frb_sea_search_each, -1);
    rb_define_method(cSearcher, "scan", frb_sea_scan, -1);
    rb_define_method(cSearcher, "explain", frb_sea_explain, 2);
    rb_define_method(cSearcher, "highlight", frb_sea_highlight, -1);
}

/*
 *  Document-class: Ferret::Search::MultiSearcher
 *
 *  == Summary
 *
 *  See Searcher for the methods that you can use on this object. A
 *  MultiSearcher is used to search multiple sub-searchers. The most efficient
 *  way to do this would be to open up an IndexReader on multiple directories
 *  and creating a Searcher with that. However, if you decide to implement a
 *  RemoteSearcher, the MultiSearcher can be used to search multiple machines
 *  at once.
 */
static void Init_MultiSearcher(void) {
    cMultiSearcher = rb_define_class_under(mSearch, "MultiSearcher", cSearcher);
    rb_define_alloc_func(cMultiSearcher, frb_ms_alloc);
    rb_define_method(cMultiSearcher, "initialize", frb_ms_init, -1);
}

/*
 *  Document-module: Ferret::Search
 *
 *  == Summary
 *
 *  The Search module contains all the classes used for searching the index;
 *  what Ferret was designed to do. The important classes to take a look at in
 *  this module are (in order);
 *
 *  * Query
 *  * Searcher
 *  * Filter
 *  * Sort
 *
 *  Happy Ferreting!!
 */
void
Init_Search(void)
{
    mSearch = rb_define_module_under(mFerret, "Search");

    fsym_id = rb_intern("id");

    Init_Hit();
    Init_TopDocs();
    Init_Explanation();

    /* Queries */
    Init_Query();

    Init_TermQuery();
    Init_MultiTermQuery();
    Init_BooleanQuery();
    Init_RangeQuery();
    Init_TypedRangeQuery();
    Init_PhraseQuery();
    Init_PrefixQuery();
    Init_WildcardQuery();
    Init_FuzzyQuery();
    Init_MatchAllQuery();
    Init_ConstantScoreQuery();
    Init_FilteredQuery();

    Init_Spans();

    /* Filters */
    Init_Filter();
    Init_RangeFilter();
    Init_TypedRangeFilter();
    Init_QueryFilter();

    /* Sorting */
    Init_SortField(); /* must be before Init_Sort */
    Init_Sort();

    /* Searchers */
    Init_Searcher();
    Init_MultiSearcher();
}
