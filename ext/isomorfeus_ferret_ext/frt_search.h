#ifndef FRT_SEARCH_H
#define FRT_SEARCH_H

#include "frt_priorityqueue.h"
#include "frt_field_index.h"
#include "frt_bitvector.h"
#include "frt_similarity.h"
#include <ruby/encoding.h>

typedef struct FrtQuery FrtQuery;
typedef struct FrtWeight FrtWeight;
typedef struct FrtScorer FrtScorer;

/***************************************************************************
 *
 * FrtExplanation
 *
 ***************************************************************************/

#define FRT_EXPLANATION_DETAILS_START_SIZE 4
typedef struct FrtExplanation {
    float value;
    char *description;
    struct FrtExplanation **details;
} FrtExplanation;

extern FrtExplanation *frt_expl_new(float value, const char *description, ...);
extern void frt_expl_destroy(FrtExplanation *expl);
extern FrtExplanation *frt_expl_add_detail(FrtExplanation *expl, FrtExplanation *detail);
extern char *frt_expl_to_s_depth(FrtExplanation *expl, int depth);
extern char *frt_expl_to_html(FrtExplanation *expl);

#define frt_expl_to_s(expl) frt_expl_to_s_depth(expl, 0)

/***************************************************************************
 *
 * Highlighter
 *
 ***************************************************************************/
typedef struct FrtSearcher FrtSearcher;

typedef struct FrtMatchRange {
    int    start;
    int    end;
    int    start_offset;
    int    end_offset;
    double score;
} FrtMatchRange;

#define FRT_MATCH_VECTOR_INIT_CAPA 8
typedef struct FrtMatchVector {
    int size;
    int capa;
    FrtMatchRange *matches;
} FrtMatchVector;

extern FrtMatchVector *frt_matchv_new();
extern FrtMatchVector *frt_matchv_add(FrtMatchVector *mp, int start, int end);
extern FrtMatchVector *frt_matchv_sort(FrtMatchVector *self);
extern void frt_matchv_destroy(FrtMatchVector *self);
extern FrtMatchVector *frt_matchv_compact(FrtMatchVector *self);
extern FrtMatchVector *frt_matchv_compact_with_breaks(FrtMatchVector *self);

/***************************************************************************
 *
 * FrtHit
 *
 ***************************************************************************/

typedef struct FrtHit {
    int   doc;
    float score;
} FrtHit;

/***************************************************************************
 *
 * FrtTopDocs
 *
 ***************************************************************************/

typedef struct FrtTopDocs {
    int    total_hits;
    int    size;
    FrtHit **hits;
    float  max_score;
} FrtTopDocs;

extern FrtTopDocs *frt_td_new(int total_hits, int size, FrtHit **hits, float max_score);
extern void frt_td_destroy(FrtTopDocs *td);
extern char *frt_td_to_s(FrtTopDocs *td);

/***************************************************************************
 *
 * FrtFilter
 *
 ***************************************************************************/

typedef struct FrtFilter {
    ID     name;
    FrtHash       *cache;
    FrtBitVector  *(*get_bv_i)(struct FrtFilter *self, FrtIndexReader *ir);
    char          *(*to_s)(struct FrtFilter *self);
    unsigned long long (*hash)(struct FrtFilter *self);
    int           (*eq)(struct FrtFilter *self, struct FrtFilter *o);
    void          (*destroy_i)(struct FrtFilter *self);
    _Atomic unsigned int   ref_cnt;
    VALUE         rfilter;
} FrtFilter;

#define filt_new(type) frt_filt_create(sizeof(type), rb_intern(#type))
extern FrtFilter *frt_filt_create(size_t size, ID name);
extern FrtBitVector *frt_filt_get_bv(FrtFilter *filt, FrtIndexReader *ir);
extern void frt_filt_destroy_i(FrtFilter *filt);
extern void frt_filt_deref(FrtFilter *filt);
extern unsigned long long frt_filt_hash(FrtFilter *filt);
extern int frt_filt_eq(FrtFilter *filt, FrtFilter *o);

/***************************************************************************
 *
 * RangeFilter
 *
 ***************************************************************************/

typedef struct FrtRange {
    ID field;
    char      *lower_term;
    char      *upper_term;
    bool      include_lower : 1;
    bool      include_upper : 1;
} FrtRange;

typedef struct FrtRangeFilter {
    FrtFilter super;
    FrtRange  *range;
} FrtRangeFilter;

extern FrtFilter *frt_rfilt_alloc(void);
extern FrtFilter *frt_rfilt_init(FrtFilter *filt, ID field, const char *lower_term, const char *upper_term,
                         bool include_lower, bool include_upper);
extern FrtFilter *frt_rfilt_new(ID field, const char *lower_term, const char *upper_term,
                         bool include_lower, bool include_upper);

/***************************************************************************
 *
 * TypedRangeFilter
 *
 ***************************************************************************/

extern FrtFilter *frt_trfilt_alloc(void);
extern FrtFilter *frt_trfilt_init(FrtFilter *filt, ID field, const char *lower_term, const char *upper_term,
                          bool include_lower, bool include_upper);
extern FrtFilter *frt_trfilt_new(ID field, const char *lower_term, const char *upper_term,
                          bool include_lower, bool include_upper);

/***************************************************************************
 *
 * QueryFilter
 *
 ***************************************************************************/

typedef struct FrtQueryFilter {
    FrtFilter super;
    FrtQuery *query;
} FrtQueryFilter;

extern FrtFilter *frt_qfilt_alloc(void);
extern FrtFilter *frt_qfilt_init(FrtFilter *filt, FrtQuery *query);
extern FrtFilter *frt_qfilt_new(FrtQuery *query);
extern FrtFilter *frt_qfilt_new_nr(FrtQuery *query);

/***************************************************************************
 *
 * FrtWeight
 *
 ***************************************************************************/

struct FrtWeight {
    float          value;
    float          qweight;
    float          qnorm;
    float          idf;
    FrtQuery       *query;
    FrtSimilarity  *similarity;
    FrtQuery       *(*get_query)(FrtWeight *self);
    float          (*get_value)(FrtWeight *self);
    void           (*normalize)(FrtWeight *self, float normalization_factor);
    FrtScorer      *(*scorer)(FrtWeight *self, FrtIndexReader *ir);
    FrtExplanation *(*explain)(FrtWeight *self, FrtIndexReader *ir, int doc_num);
    float          (*sum_of_squared_weights)(FrtWeight *self);
    char           *(*to_s)(FrtWeight *self);
    void           (*destroy)(FrtWeight *self);
};

#define w_new(type, query) frt_w_create(sizeof(type), query)
extern FrtWeight *frt_w_create(size_t size, FrtQuery *query);
extern void frt_w_destroy(FrtWeight *self);
extern FrtQuery *frt_w_get_query(FrtWeight *self);
extern float frt_w_get_value(FrtWeight *self);
extern float frt_w_sum_of_squared_weights(FrtWeight *self);
extern void frt_w_normalize(FrtWeight *self, float normalization_factor);

/***************************************************************************
 *
 * FrtQuery
 *
 ***************************************************************************/

typedef enum {
    TERM_QUERY,
    MULTI_TERM_QUERY,
    BOOLEAN_QUERY,
    PHRASE_QUERY,
    CONSTANT_QUERY,
    FILTERED_QUERY,
    MATCH_ALL_QUERY,
    RANGE_QUERY,
    TYPED_RANGE_QUERY,
    WILD_CARD_QUERY,
    FUZZY_QUERY,
    PREFIX_QUERY,
    SPAN_TERM_QUERY,
    SPAN_MULTI_TERM_QUERY,
    SPAN_PREFIX_QUERY,
    SPAN_FIRST_QUERY,
    SPAN_OR_QUERY,
    SPAN_NOT_QUERY,
    SPAN_NEAR_QUERY
} FrtQueryType;

struct FrtQuery {
    _Atomic unsigned int        ref_cnt;
    float              boost;
    FrtWeight          *weight;
    FrtQuery           *(*rewrite)(FrtQuery *self, FrtIndexReader *ir);
    void               (*extract_terms)(FrtQuery *self, FrtHashSet *terms);
    FrtSimilarity      *(*get_similarity)(FrtQuery *self, FrtSearcher *searcher);
    char               *(*to_s)(FrtQuery *self, ID field);
    unsigned long long (*hash)(FrtQuery *self);
    int                (*eq)(FrtQuery *self, FrtQuery *o);
    void               (*destroy_i)(FrtQuery *self);
    FrtWeight          *(*create_weight_i)(FrtQuery *self, FrtSearcher *searcher);
    FrtMatchVector     *(*get_matchv_i)(FrtQuery *self, FrtMatchVector *mv, FrtTermVector *tv);
    FrtQueryType       type;
    VALUE              rquery;
};

/* Internal FrtQuery Functions */
extern FrtSimilarity *frt_q_get_similarity_i(FrtQuery *self, FrtSearcher *searcher);
extern void frt_q_destroy_i(FrtQuery *self);
extern FrtWeight *frt_q_create_weight_unsup(FrtQuery *self, FrtSearcher *searcher);

extern void frt_q_deref(FrtQuery *self);
extern const char *frt_q_get_query_name(FrtQueryType type);
extern FrtWeight *frt_q_weight(FrtQuery *self, FrtSearcher *searcher);
extern FrtQuery *frt_q_combine(FrtQuery **queries, int q_cnt);
extern unsigned long long frt_q_hash(FrtQuery *self);
extern int frt_q_eq(FrtQuery *self, FrtQuery *o);
extern FrtQuery *frt_q_create(size_t size);
#define frt_q_new(type) frt_q_create(sizeof(type))

/***************************************************************************
 * FrtTermQuery
 ***************************************************************************/

typedef struct FrtTermQuery {
    FrtQuery  super;
    ID        field;
    char      *term;
} FrtTermQuery;

FrtQuery *frt_tq_alloc(void);
FrtQuery *frt_tq_init(FrtQuery *self, ID field, const char *term);
FrtQuery *frt_tq_new(ID field, const char *term);

/***************************************************************************
 * FrtBooleanQuery
 ***************************************************************************/

typedef enum {
    FRT_BC_SHOULD,
    FRT_BC_MUST,
    FRT_BC_MUST_NOT
} FrtBCType;

typedef struct FrtBooleanClause {
    _Atomic unsigned int ref_cnt;
    FrtQuery    *query;
    FrtBCType   occur;
    bool        is_prohibited : 1;
    bool        is_required : 1;
    VALUE       rbc;
} FrtBooleanClause;

extern FrtBooleanClause *frt_bc_alloc(void);
extern FrtBooleanClause *frt_bc_init(FrtBooleanClause *self, FrtQuery *query, FrtBCType occur);
extern FrtBooleanClause *frt_bc_new(FrtQuery *query, FrtBCType occur);
extern void frt_bc_deref(FrtBooleanClause *self);
extern void frt_bc_set_occur(FrtBooleanClause *self, FrtBCType occur);

/* *** FrtBooleanQuery *** */

#define FRT_DEFAULT_MAX_CLAUSE_COUNT 1024
#define FRT_BOOLEAN_CLAUSES_START_CAPA 4
#define FRT_QUERY_STRING_START_SIZE 64

typedef struct FrtBooleanQuery {
    FrtQuery         super;
    bool             coord_disabled;
    int              max_clause_cnt;
    int              clause_cnt;
    int              clause_capa;
    float            original_boost;
    FrtBooleanClause **clauses;
    FrtSimilarity    *similarity;
} FrtBooleanQuery;

extern FrtQuery *frt_bq_alloc(void);
extern FrtQuery *frt_bq_init(FrtQuery *self, bool coord_disabled);
extern FrtQuery *frt_bq_new(bool coord_disabled);
extern FrtQuery *frt_bq_new_max(bool coord_disabled, int max);
extern FrtBooleanClause *frt_bq_add_query(FrtQuery *self, FrtQuery *sub_query, FrtBCType occur);
extern FrtBooleanClause *frt_bq_add_query_nr(FrtQuery *self, FrtQuery *sub_query, FrtBCType occur);
extern FrtBooleanClause *frt_bq_add_clause(FrtQuery *self, FrtBooleanClause *bc);
extern FrtBooleanClause *frt_bq_add_clause_nr(FrtQuery *self, FrtBooleanClause *bc);

/***************************************************************************
 * FrtPhraseQuery
 ***************************************************************************/

typedef struct FrtPhraseQuery {
    FrtQuery          super;
    int               slop;
    ID                field;
    FrtPhrasePosition *positions;
    int               pos_cnt;
    int               pos_capa;
} FrtPhraseQuery;

extern FrtQuery *frt_phq_alloc(void);
extern FrtQuery *frt_phq_init(FrtQuery *self, ID field);
extern FrtQuery *frt_phq_new(ID field);
extern void frt_phq_add_term(FrtQuery *self, const char *term, int pos_inc);
extern void frt_phq_add_term_abs(FrtQuery *self, const char *term, int position);
extern void frt_phq_append_multi_term(FrtQuery *self, const char *term);
extern void frt_phq_set_slop(FrtQuery *self, int slop);

/***************************************************************************
 * FrtMultiTermQuery
 ***************************************************************************/

#define MULTI_TERM_QUERY_MAX_TERMS 256
typedef struct FrtMultiTermQuery {
    FrtQuery         super;
    ID               field;
    FrtPriorityQueue *boosted_terms;
    float            min_boost;
} FrtMultiTermQuery;

extern void frt_multi_tq_add_term(FrtQuery *self, const char *term);
extern void frt_multi_tq_add_term_boost(FrtQuery *self, const char *term, float boost);
extern FrtQuery *frt_multi_tq_alloc(void);
extern FrtQuery *frt_multi_tq_new(ID field);
extern FrtQuery *frt_multi_tq_init_conf(FrtQuery *self, ID field, int max_terms, float min_boost);
extern FrtQuery *frt_multi_tq_new_conf(ID field, int max_terms, float min_boost);

#define FrtMTQMaxTerms(query) (((FrtMTQSubQuery *)(query))->max_terms)
typedef struct FrtMTQSubQuery {
    FrtQuery super;
    int      max_terms;
} FrtMTQSubQuery;

/***************************************************************************
 * FrtPrefixQuery
 ***************************************************************************/

#define PREFIX_QUERY_MAX_TERMS 256

typedef struct FrtPrefixQuery {
    FrtMTQSubQuery super;
    ID             field;
    char           *prefix;
} FrtPrefixQuery;

extern FrtQuery *frt_prefixq_alloc(void);
extern FrtQuery *frt_prefixq_init(FrtQuery *self, ID field, const char *prefix);
extern FrtQuery *frt_prefixq_new(ID field, const char *prefix);

/***************************************************************************
 * FrtWildCardQuery
 ***************************************************************************/

#define FRT_WILD_CHAR '?'
#define FRT_WILD_STRING '*'
#define FRT_WILD_CARD_QUERY_MAX_TERMS 256

typedef struct FrtWildCardQuery {
    FrtMTQSubQuery super;
    ID             field;
    char           *pattern;
} FrtWildCardQuery;

extern FrtQuery *frt_wcq_alloc(void);
extern FrtQuery *frt_wcq_init(FrtQuery *self, ID field, const char *pattern);
extern FrtQuery *frt_wcq_new(ID field, const char *pattern);
extern bool frt_wc_match(const char *pattern, const char *text);

/***************************************************************************
 * FrtFuzzyQuery
 ***************************************************************************/

#define FRT_DEF_MIN_SIM 0.5f
#define FRT_DEF_PRE_LEN 0
#define FRT_DEF_MAX_TERMS 256
#define FRT_TYPICAL_LONGEST_WORD 20

typedef struct FrtFuzzyQuery {
    FrtMTQSubQuery super;
    ID             field;
    char           *term;
    const char     *text; /* term text after prefix */
    int            text_len;
    int            pre_len;
    float          min_sim;
    float          scale_factor;
    int            max_distances[FRT_TYPICAL_LONGEST_WORD];
    int            *da;
} FrtFuzzyQuery;

extern FrtQuery *frt_fuzq_alloc(void);
extern FrtQuery *frt_fuzq_new(ID field, const char *term);
extern FrtQuery *frt_fuzq_init_conf(FrtQuery *self, ID field, const char *term, float min_sim, int pre_len, int max_terms);
extern FrtQuery *frt_fuzq_new_conf(ID field, const char *term, float min_sim, int pre_len, int max_terms);
extern float frt_fuzq_score(FrtFuzzyQuery *fuzq, const char *target);

/***************************************************************************
 * FrtConstantScoreQuery
 ***************************************************************************/

typedef struct FrtConstantScoreQuery {
    FrtQuery  super;
    FrtFilter *filter;
    FrtQuery  *original;
} FrtConstantScoreQuery;

extern FrtQuery *frt_csq_alloc(void);
extern FrtQuery *frt_csq_init(FrtQuery *self, FrtFilter *filter);
extern FrtQuery *frt_csq_new(FrtFilter *filter);
extern FrtQuery *frt_csq_new_nr(FrtFilter *filter);

/***************************************************************************
 * FrtFilteredQuery
 ***************************************************************************/

typedef struct FrtFilteredQuery {
    FrtQuery  super;
    FrtQuery  *query;
    FrtFilter *filter;
} FrtFilteredQuery;

extern FrtQuery *frt_fq_alloc(void);
extern FrtQuery *frt_fq_init(FrtQuery *self, FrtQuery *query, FrtFilter *filter);
extern FrtQuery *frt_fq_new(FrtQuery *query, FrtFilter *filter);

/***************************************************************************
 * FrtMatchAllQuery
 ***************************************************************************/

extern FrtQuery *frt_maq_alloc(void);
extern FrtQuery *frt_maq_init(FrtQuery *self);
extern FrtQuery *frt_maq_new();

/***************************************************************************
 * FrtRangeQuery
 ***************************************************************************/

typedef struct FrtRangeQuery {
    FrtQuery f;
    FrtRange *range;
} FrtRangeQuery;

extern FrtQuery *frt_rq_alloc(void);
extern FrtQuery *frt_rq_init(FrtQuery *self, ID field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper);
extern FrtQuery *frt_rq_new(ID field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper);
extern FrtQuery *frt_rq_new_less(ID field, const char *upper_term, bool include_upper);
extern FrtQuery *frt_rq_new_more(ID field, const char *lower_term, bool include_lower);

/***************************************************************************
 * FrtTypedRangeQuery
 ***************************************************************************/

extern FrtQuery *frt_trq_alloc(void);
extern FrtQuery *frt_trq_init(FrtQuery *self, ID field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper);
extern FrtQuery *frt_trq_new(ID field, const char *lower_term, const char *upper_term, bool include_lower, bool include_upper);
extern FrtQuery *frt_trq_new_less(ID field, const char *upper_term, bool include_upper);
extern FrtQuery *frt_trq_new_more(ID field, const char *lower_term, bool include_lower);

/***************************************************************************
 * FrtSpanQuery
 ***************************************************************************/

/* ** FrtSpanEnum ** */
typedef struct FrtSpanEnum FrtSpanEnum;
struct FrtSpanEnum {
    FrtQuery *query;
    bool     (*next)(FrtSpanEnum *self);
    bool     (*skip_to)(FrtSpanEnum *self, int target_doc);
    int      (*doc)(FrtSpanEnum *self);
    int      (*start)(FrtSpanEnum *self);
    int      (*end)(FrtSpanEnum *self);
    char     *(*to_s)(FrtSpanEnum *self);
    void     (*destroy)(FrtSpanEnum *self);
};

/* ** FrtSpanQuery ** */
typedef struct FrtSpanQuery {
    FrtQuery    super;
    ID          field;
    FrtSpanEnum *(*get_spans)(FrtQuery *self, FrtIndexReader *ir);
    FrtHashSet  *(*get_terms)(FrtQuery *self);
} FrtSpanQuery;

/***************************************************************************
 * FrtSpanTermQuery
 ***************************************************************************/

typedef struct FrtSpanTermQuery {
    FrtSpanQuery super;
    char         *term;
} FrtSpanTermQuery;

extern FrtQuery *frt_spantq_alloc(void);
extern FrtQuery *frt_spantq_init(FrtQuery *self, ID field, const char *term);
extern FrtQuery *frt_spantq_new(ID field, const char *term);

/***************************************************************************
 * FrtSpanMultiTermQuery
 ***************************************************************************/

#define SPAN_MULTI_TERM_QUERY_CAPA 1024
typedef struct FrtSpanMultiTermQuery {
    FrtSpanQuery super;
    char         **terms;
    int          term_cnt;
    int          term_capa;
} FrtSpanMultiTermQuery;

extern FrtQuery *frt_spanmtq_alloc(void);
extern FrtQuery *frt_spanmtq_init(FrtQuery *self, ID field);
extern FrtQuery *frt_spanmtq_new(ID field);
extern FrtQuery *frt_spanmtq_new_conf(ID field, int max_size);
extern void frt_spanmtq_add_term(FrtQuery *self, const char *term);

/***************************************************************************
 * FrtSpanFirstQuery
 ***************************************************************************/

typedef struct FrtSpanFirstQuery {
    FrtSpanQuery super;
    int          end;
    FrtQuery     *match;
} FrtSpanFirstQuery;

extern FrtQuery *frt_spanfq_alloc(void);
extern FrtQuery *frt_spanfq_init(FrtQuery *self, FrtQuery *match, int end);
extern FrtQuery *frt_spanfq_new(FrtQuery *match, int end);
extern FrtQuery *frt_spanfq_new_nr(FrtQuery *match, int end);

/***************************************************************************
 * FrtSpanOrQuery
 ***************************************************************************/

typedef struct FrtSpanOrQuery {
    FrtSpanQuery super;
    FrtQuery     **clauses;
    int          c_cnt;
    int          c_capa;
} FrtSpanOrQuery;

extern FrtQuery *frt_spanoq_alloc(void);
extern FrtQuery *frt_spanoq_init(FrtQuery *self);
extern FrtQuery *frt_spanoq_new();
extern FrtQuery *frt_spanoq_add_clause(FrtQuery *self, FrtQuery *clause);
extern FrtQuery *frt_spanoq_add_clause_nr(FrtQuery *self, FrtQuery *clause);

/***************************************************************************
 * FrtSpanNearQuery
 ***************************************************************************/

typedef struct FrtSpanNearQuery {
    FrtSpanQuery super;
    FrtQuery     **clauses;
    int          c_cnt;
    int          c_capa;
    int          slop;
    bool         in_order : 1;
} FrtSpanNearQuery;

extern FrtQuery *frt_spannq_alloc(void);
extern FrtQuery *frt_spannq_init(FrtQuery *self, int slop, bool in_order);
extern FrtQuery *frt_spannq_new(int slop, bool in_order);
extern FrtQuery *frt_spannq_add_clause(FrtQuery *self, FrtQuery *clause);
extern FrtQuery *frt_spannq_add_clause_nr(FrtQuery *self, FrtQuery *clause);

/***************************************************************************
 * FrtSpanNotQuery
 ***************************************************************************/

typedef struct FrtSpanNotQuery {
    FrtSpanQuery super;
    FrtQuery     *inc;
    FrtQuery     *exc;
} FrtSpanNotQuery;

extern FrtQuery *frt_spanxq_alloc(void);
extern FrtQuery *frt_spanxq_init(FrtQuery *self, FrtQuery *inc, FrtQuery *exc);
extern FrtQuery *frt_spanxq_new(FrtQuery *inc, FrtQuery *exc);
extern FrtQuery *frt_spanxq_new_nr(FrtQuery *inc, FrtQuery *exc);

/***************************************************************************
 * FrtSpanPrefixQuery
 ***************************************************************************/

#define FRT_SPAN_PREFIX_QUERY_MAX_TERMS 256

typedef struct FrtSpanPrefixQuery {
    FrtSpanQuery super;
    char         *prefix;
    int          max_terms;
} FrtSpanPrefixQuery;

extern FrtQuery *frt_spanprq_alloc(void);
extern FrtQuery *frt_spanprq_init(FrtQuery *self, ID field, const char *prefix);
extern FrtQuery *frt_spanprq_new(ID field, const char *prefix);

/***************************************************************************
 *
 * FrtScorer
 *
 ***************************************************************************/

#define FRT_SCORER_NULLIFY(mscorer) do {\
    (mscorer)->destroy(mscorer);\
    (mscorer) = NULL;\
} while (0)

struct FrtScorer {
    FrtSimilarity  *similarity;
    int            doc;
    float          (*score)(FrtScorer *self);
    bool           (*next)(FrtScorer *self);
    bool           (*skip_to)(FrtScorer *self, int doc_num);
    FrtExplanation *(*explain)(FrtScorer *self, int doc_num);
    void           (*destroy)(FrtScorer *self);
};

#define frt_scorer_new(type, similarity) frt_scorer_create(sizeof(type), similarity)
/* Internal FrtScorer Function */
extern void frt_scorer_destroy_i(FrtScorer *self);
extern FrtScorer *frt_scorer_create(size_t size, FrtSimilarity *similarity);
extern bool frt_scorer_doc_less_than(const FrtScorer *s1, const FrtScorer *s2);
extern int frt_scorer_doc_cmp(const void *p1, const void *p2);

/***************************************************************************
 * FrtComparable
 ***************************************************************************/

typedef struct FrtComparable {
    int       type;
    union {
        long  l;
        float f;
        char  *s;
        void  *p;
    } val;
    bool      reverse : 1;
} FrtComparable;

/***************************************************************************
 *
 * FrtSort
 *
 ***************************************************************************/

typedef enum {
    FRT_SORT_TYPE_SCORE,
    FRT_SORT_TYPE_DOC,
    FRT_SORT_TYPE_BYTE,
    FRT_SORT_TYPE_INTEGER,
    FRT_SORT_TYPE_FLOAT,
    FRT_SORT_TYPE_STRING,
    FRT_SORT_TYPE_AUTO
} SortType;

/***************************************************************************
 * FrtSortField
 ***************************************************************************/

typedef struct FrtSortField {
    const FrtFieldIndexClass *field_index_class;
    ID       field;
    SortType type;
    bool     reverse : 1;
    int      (*compare)(void *index_ptr, FrtHit *hit1, FrtHit *hit2);
    void     (*get_val)(void *index_ptr, FrtHit *hit1, FrtComparable *comparable);
    VALUE    rfield;
} FrtSortField;

extern FrtSortField *frt_sort_field_alloc(void);
extern FrtSortField *frt_sort_field_init(FrtSortField *self, ID field, SortType type, bool reverse);
extern FrtSortField *frt_sort_field_new(ID field, SortType type, bool reverse);
extern FrtSortField *frt_sort_field_score_init(FrtSortField *self, bool reverse);
extern FrtSortField *frt_sort_field_score_new(bool reverse);
extern void frt_sort_field_score_get_val(void *index, FrtHit *hit, FrtComparable *comparable);
extern int frt_sort_field_score_compare(void *index_ptr, FrtHit *hit2, FrtHit *hit1);
extern FrtSortField *frt_sort_field_doc_init(FrtSortField *self, bool reverse);
extern FrtSortField *frt_sort_field_doc_new(bool reverse);
extern void frt_sort_field_doc_get_val(void *index, FrtHit *hit, FrtComparable *comparable);
extern int frt_sort_field_doc_compare(void *index_ptr, FrtHit *hit1, FrtHit *hit2);
extern FrtSortField *frt_sort_field_int_init(FrtSortField *self, ID field, bool reverse);
extern FrtSortField *frt_sort_field_int_new(ID field, bool reverse);
extern FrtSortField *frt_sort_field_byte_init(FrtSortField *self, ID field, bool reverse);
extern FrtSortField *frt_sort_field_byte_new(ID field, bool reverse);
extern FrtSortField *frt_sort_field_float_init(FrtSortField *self, ID field, bool reverse);
extern FrtSortField *frt_sort_field_float_new(ID field, bool reverse);
extern FrtSortField *frt_sort_field_string_init(FrtSortField *self, ID field, bool reverse);
extern FrtSortField *frt_sort_field_string_new(ID field, bool reverse);
extern FrtSortField *frt_sort_field_auto_init(FrtSortField *self, ID field, bool reverse);
extern FrtSortField *frt_sort_field_auto_new(ID field, bool reverse);
extern void frt_sort_field_destroy(void *p);
extern char *frt_sort_field_to_s(FrtSortField *self);

extern FrtSortField *FRT_SORT_FIELD_SCORE;
extern FrtSortField *FRT_SORT_FIELD_SCORE_REV;
extern FrtSortField *FRT_SORT_FIELD_DOC;
extern FrtSortField *FRT_SORT_FIELD_DOC_REV;

/***************************************************************************
 * FrtSort
 ***************************************************************************/

typedef struct FrtSort {
    FrtSortField **sort_fields;
    int  size;
    int  capa;
    int  start;
    bool destroy_all : 1;
} FrtSort;

extern FrtSort *frt_sort_new();
extern void frt_sort_destroy(void *p);
extern void frt_sort_add_sort_field(FrtSort *self, FrtSortField *sf);
extern void frt_sort_clear(FrtSort *self);
extern char *frt_sort_to_s(FrtSort *self);

/***************************************************************************
 * FieldSortedHitQueue
 ***************************************************************************/

extern FrtHit *frt_fshq_pq_pop(FrtPriorityQueue *pq);
extern void frt_fshq_pq_insert(FrtPriorityQueue *pq, FrtHit *hit);
extern void frt_fshq_pq_destroy(FrtPriorityQueue *pq);
extern FrtPriorityQueue *frt_fshq_pq_new(int size, FrtSort *sort, FrtIndexReader *ir);
extern FrtHit *frt_fshq_pq_pop_fd(FrtPriorityQueue *pq);

/***************************************************************************
 * FrtFieldDoc
 ***************************************************************************/

typedef struct FrtFieldDoc {
    FrtHit        hit;
    int           size;
    FrtComparable comparables[1];
} FrtFieldDoc;

/***************************************************************************
 * FieldDocSortedHitQueue
 ***************************************************************************/

extern bool frt_fdshq_lt(FrtFieldDoc *fd1, FrtFieldDoc *fd2);

/***************************************************************************
 *
 * FrtSearcher
 *
 ***************************************************************************/

typedef struct FrtPostFilter {
    float (*filter_func)(int doc_num, float score, FrtSearcher *self, void *arg);
    void  *arg;
} FrtPostFilter;

struct FrtSearcher {
    _Atomic unsigned int    ref_cnt;
    FrtSimilarity  *similarity;
    int            (*doc_freq)(FrtSearcher *self, ID field, const char *term);
    FrtDocument    *(*get_doc)(FrtSearcher *self, int doc_num);
    FrtLazyDoc     *(*get_lazy_doc)(FrtSearcher *self, int doc_num);
    int            (*max_doc)(FrtSearcher *self);
    FrtWeight      *(*create_weight)(FrtSearcher *self, FrtQuery *query);
    FrtTopDocs     *(*search)(FrtSearcher *self, FrtQuery *query, int first_doc, int num_docs, FrtFilter *filter, FrtSort *sort, FrtPostFilter *post_filter, bool load_fields);
    FrtTopDocs     *(*search_w)(FrtSearcher *self, FrtWeight *weight, int first_doc, int num_docs, FrtFilter *filter, FrtSort *sort, FrtPostFilter *post_filter, bool load_fields);
    void           (*search_each)(FrtSearcher *self, FrtQuery *query, FrtFilter *filter, FrtPostFilter *post_filter, void (*fn)(FrtSearcher *, int, float, void *), void *arg);
    void           (*search_each_w)(FrtSearcher *self, FrtWeight *weight, FrtFilter *filter, FrtPostFilter *post_filter, void (*fn)(FrtSearcher *, int, float, void *), void *arg);
    /*
     * Scan the index for all documents that match a query and write the
     * results to a buffer. It will stop scanning once the limit is reached
     * and it starts scanning from offset_docnum.
     *
     * Note: Unlike the offset_docnum in other search methods, this
     * offset_docnum refers to document number and not hit.
     */
    int            (*search_unscored)(FrtSearcher *searcher, FrtQuery *query, int *buf, int limit, int offset_docnum);
    int            (*search_unscored_w)(FrtSearcher *searcher, FrtWeight *weight, int *buf, int limit, int offset_docnum);
    FrtQuery       *(*rewrite)(FrtSearcher *self, FrtQuery *original);
    FrtExplanation *(*explain)(FrtSearcher *self, FrtQuery *query, int doc_num);
    FrtExplanation *(*explain_w)(FrtSearcher *self, FrtWeight *weight, int doc_num);
    FrtTermVector  *(*get_term_vector)(FrtSearcher *self, const int doc_num, ID field);
    FrtSimilarity  *(*get_similarity)(FrtSearcher *self);
    void           (*close)(FrtSearcher *self);
    VALUE          rsea;
};

#define frt_searcher_get_doc(s, dn)         s->get_doc(s, dn)
#define frt_searcher_get_lazy_doc(s, dn)    s->get_lazy_doc(s, dn)
#define frt_searcher_max_doc(s)             s->max_doc(s)
#define frt_searcher_rewrite(s, q)          s->rewrite(s, q)
#define frt_searcher_explain(s, q, dn)      s->explain(s, q, dn)
#define frt_searcher_close(s)               s->close(s)
#define frt_searcher_search(s, q, fd, nd, filt, sort, ff)   s->search(s, q, fd, nd, filt, sort, ff, false)
#define frt_searcher_search_each(s, q, filt, ff, fn, arg)   s->search_each(s, q, filt, ff, fn, arg)
#define frt_searcher_search_unscored(s, q, buf, limit, offset_docnum)   s->search_unscored(s, q, buf, limit, offset_docnum)

extern FrtMatchVector *frt_searcher_get_match_vector(FrtSearcher *self, FrtQuery *query, const int doc_num, ID field);
extern char **frt_searcher_highlight(FrtSearcher *self,
                                 FrtQuery *query,
                                 const int doc_num,
                                 ID field,
                                 const int excerpt_len,
                                 const int num_excerpts,
                                 const char *pre_tag,
                                 const char *post_tag,
                                 const char *ellipsis);

/***************************************************************************
 *
 * FrtIndexSearcher
 *
 ***************************************************************************/

typedef struct FrtIndexSearcher {
    FrtSearcher    super;
    FrtIndexReader *ir;
} FrtIndexSearcher;

extern FrtSearcher *frt_isea_alloc(void);
extern FrtSearcher *frt_isea_init(FrtSearcher *self, FrtIndexReader *ir);
extern FrtSearcher *frt_isea_new(FrtIndexReader *ir);
extern int frt_isea_doc_freq(FrtSearcher *self, ID field, const char *term);

/***************************************************************************
 *
 * FrtMultiSearcher
 *
 ***************************************************************************/

typedef struct FrtMultiSearcher {
    FrtSearcher super;
    int         s_cnt;
    FrtSearcher **searchers;
    int         *starts;
    int         max_doc;
} FrtMultiSearcher;

extern FrtSearcher *frt_msea_alloc(void);
extern FrtSearcher *frt_msea_init(FrtSearcher *self, FrtSearcher **searchers, int s_cnt);
extern FrtSearcher *frt_msea_new(FrtSearcher **searchers, int s_cnt);

/***************************************************************************
 *
 * FrtQParser
 *
 ***************************************************************************/

#define FRT_QP_CONC_WORDS 2
#define FRT_QP_MAX_CLAUSES 512
typedef struct FrtFieldStack {
    FrtHashSet           *fields;
    struct FrtFieldStack *next;
    bool                 destroy : 1;
} FrtFieldStack;

typedef struct FrtQueryParser {
    frt_mutex_t    mutex;
    int            def_slop;
    int            max_clauses;
    int            phq_pos_inc;
    char           *qstr;
    char           *qstrp;
    char           buf[FRT_QP_CONC_WORDS][FRT_MAX_WORD_SIZE];
    char           *dynbuf;
    int            buf_index;
    FrtHashSet     *def_fields;
    FrtHashSet     *all_fields;
    FrtHashSet     *tokenized_fields;
    FrtHashSet     *fields;
    FrtFieldStack  *fields_top;
    FrtAnalyzer    *analyzer;
    FrtHash        *ts_cache;
    FrtQuery       *result;
    FrtTokenStream *non_tokenizer;
    bool           or_default : 1;
    bool           wild_lower : 1;
    bool           clean_str : 1;
    bool           handle_parse_errors : 1;
    bool           allow_any_fields : 1;
    bool           destruct : 1;
    bool           recovering : 1;
    bool           use_keywords : 1;
    bool           use_typed_range_query : 1;
} FrtQueryParser;
typedef FrtQueryParser FrtQParser; /* FrtQParser is an alias for FrtQueryParser */

extern FrtQParser *frt_qp_alloc(void);
extern FrtQParser *frt_qp_init(FrtQParser *, FrtAnalyzer *analyzer);
extern FrtQParser *frt_qp_new(FrtAnalyzer *analyzer);
extern void frt_qp_add_field(FrtQParser *self, ID field, bool is_default, bool is_tokenized);
extern void frt_qp_destroy(FrtQParser *self);
extern FrtQuery *qp_parse(FrtQParser *self, char *qstr, rb_encoding *encoding);
extern char *frt_qp_clean_str(char *str);

extern float frt_qp_default_fuzzy_min_sim;
extern int frt_qp_default_fuzzy_pre_len;

#endif
