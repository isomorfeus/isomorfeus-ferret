#include "ruby.h"
#include "frt_ind.h"
#include "frt_array.h"
#include <string.h>
#include "frt_internal.h"

static const char *NON_UNIQUE_KEY_ERROR_MSG =
    "Tried to use a key that was not unique";

#define INDEX_CLOSE_READER(self) do { \
    if (self->sea) {                  \
        searcher_close(self->sea);    \
        self->sea = NULL;             \
        self->ir = NULL;              \
    } else if (self->ir) {            \
        ir_close(self->ir);           \
        self->ir = NULL;              \
    }                                 \
} while (0)

#define AUTOFLUSH_IR(self) do {                 \
     if (self->auto_flush) ir_commit(self->ir); \
    else self->has_writes = true;               \
} while(0)

#define AUTOFLUSH_IW(self) do {  \
    if (self->auto_flush) {      \
        iw_close(self->iw);      \
        self->iw = NULL;         \
    } else {                     \
        self->has_writes = true; \
    }                            \
} while (0)

void index_auto_flush_ir(Index *self)
{
    AUTOFLUSH_IR(self);
}

void index_auto_flush_iw(Index *self)
{
    AUTOFLUSH_IW(self);
}


Index *index_new(Store *store, FrtAnalyzer *analyzer, HashSet *def_fields,
                 bool create)
{
    Index *self = FRT_ALLOC_AND_ZERO(Index);
    HashSetEntry *hse;
    /* FIXME: need to add these to the query parser */
    self->config = default_config;
    mutex_init(&self->mutex, NULL);
    self->has_writes = false;
    if (store) {
        FRT_REF(store);
        self->store = store;
    } else {
        self->store = open_ram_store();
        create = true;
    }
    if (analyzer) {
        self->analyzer = analyzer;
        FRT_REF(analyzer);
    } else {
        self->analyzer = mb_standard_analyzer_new(true);
    }

    if (create) {
        FrtFieldInfos *fis = fis_new(FRT_STORE_YES, FRT_INDEX_YES,
                                  FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS);
        index_create(self->store, fis);
        fis_deref(fis);
    }

    /* options */
    self->key = NULL;
    self->id_field = "id";
    self->def_field = "id";
    self->auto_flush = false;
    self->check_latest = true;

    FRT_REF(self->analyzer);
    self->qp = qp_new(self->analyzer);
    for (hse = def_fields->first; hse; hse = hse->next) {
        qp_add_field(self->qp, (Symbol)hse->elem, true, true);
    }
    /* Index is a convenience class so set qp convenience options */
    self->qp->allow_any_fields = true;
    self->qp->clean_str = true;
    self->qp->handle_parse_errors = true;

    return self;
}

void index_destroy(Index *self)
{
    mutex_destroy(&self->mutex);
    INDEX_CLOSE_READER(self);
    if (self->iw) iw_close(self->iw);
    store_deref(self->store);
    a_deref(self->analyzer);
    if (self->qp) qp_destroy(self->qp);
    if (self->key) hs_destroy(self->key);
    free(self);
}

void index_flush(Index *self)
{
    if (self->ir) {
        ir_commit(self->ir);
    } else if (self->iw) {
        iw_close(self->iw);
        self->iw = NULL;
    }
    self->has_writes = false;
}

void ensure_writer_open(Index *self)
{
    if (!self->iw) {
        INDEX_CLOSE_READER(self);

        /* make sure the analzyer isn't deleted by the IndexWriter */
        FRT_REF(self->analyzer);
        self->iw = iw_open(self->store, self->analyzer, false);
        self->iw->config.use_compound_file = self->config.use_compound_file;
    }
}

void ensure_reader_open(Index *self)
{
    if (self->ir) {
        if (self->check_latest && !ir_is_latest(self->ir)) {
            INDEX_CLOSE_READER(self);
            self->ir = ir_open(self->store);
        }
        return;
    }
    if (self->iw) {
        iw_close(self->iw);
        self->iw = NULL;
    }
    self->ir = ir_open(self->store);
}

void ensure_searcher_open(Index *self)
{
    ensure_reader_open(self);
    if (!self->sea) {
        self->sea = isea_new(self->ir);
    }
}

int index_size(Index *self)
{
    int size;
    mutex_lock(&self->mutex);
    {
        ensure_reader_open(self);
        size = self->ir->num_docs(self->ir);
    }
    mutex_unlock(&self->mutex);
    return size;
}

void index_optimize(Index *self)
{
    mutex_lock(&self->mutex);
    {
        ensure_writer_open(self);
        iw_optimize(self->iw);
        AUTOFLUSH_IW(self);
    }
    mutex_unlock(&self->mutex);
}

bool index_has_del(Index *self)
{
    bool has_del;
    mutex_lock(&self->mutex);
    {
        ensure_reader_open(self);
        has_del = self->ir->has_deletions(self->ir);
    }
    mutex_unlock(&self->mutex);
    return has_del;
}

bool index_is_deleted(Index *self, int doc_num)
{
    bool is_del;
    mutex_lock(&self->mutex);
    {
        ensure_reader_open(self);
        is_del = self->ir->is_deleted(self->ir, doc_num);
    }
    mutex_unlock(&self->mutex);
    return is_del;
}

static void index_del_doc_with_key_i(Index *self, FrtDocument *doc,
                                            HashSet *key)
{
    Query *q;
    TopDocs *td;
    FrtDocField *df;
    HashSetEntry *hse;

    if (key->size == 1) {
        Symbol field = (Symbol)key->first->elem;
        ensure_writer_open(self);
        df = doc_get_field(doc, field);
        if (df) {
            iw_delete_term(self->iw, field, df->data[0]);
        }
        return;
    }

    q = bq_new(false);
    ensure_searcher_open(self);

    for (hse = key->first; hse; hse = hse->next) {
        Symbol field = (Symbol)hse->elem;
        df = doc_get_field(doc, field);
        if (!df) continue;
        bq_add_query(q, tq_new(field, df->data[0]), FRT_BC_MUST);
    }
    td = searcher_search(self->sea, q, 0, 1, NULL, NULL, NULL);
    if (td->total_hits > 1) {
        td_destroy(td);
        rb_raise(rb_eArgError, "%s", NON_UNIQUE_KEY_ERROR_MSG);
    } else if (td->total_hits == 1) {
        ir_delete_doc(self->ir, td->hits[0]->doc);
    }
    q_deref(q);
    td_destroy(td);
}

static void index_add_doc_i(Index *self, FrtDocument *doc)
{
    if (self->key) {
        index_del_doc_with_key_i(self, doc, self->key);
    }
    ensure_writer_open(self);
    iw_add_doc(self->iw, doc);
    AUTOFLUSH_IW(self);
}

void index_add_doc(Index *self, FrtDocument *doc)
{
    mutex_lock(&self->mutex);
    {
        index_add_doc_i(self, doc);
    }
    mutex_unlock(&self->mutex);
}

void index_add_string(Index *self, char *str)
{
    FrtDocument *doc = doc_new();
    doc_add_field(doc, df_add_data(df_new(self->def_field), estrdup(str)));
    index_add_doc(self, doc);
    doc_destroy(doc);
}

void index_add_array(Index *self, char **fields)
{
    int i;
    FrtDocument *doc = doc_new();
    for (i = 0; i < ary_size(fields); i++) {
        doc_add_field(doc, df_add_data(df_new(self->def_field),
                                       estrdup(fields[i])));
    }
    index_add_doc(self, doc);
    doc_destroy(doc);
}

Query *index_get_query(Index *self, char *qstr)
{
    int i;
    FrtFieldInfos *fis;
    ensure_searcher_open(self);
    fis = self->ir->fis;
    for (i = fis->size - 1; i >= 0; i--) {
        hs_add(self->qp->all_fields, strdup(fis->fields[i]->name));
    }
    return qp_parse(self->qp, qstr);
}

TopDocs *index_search_str(Index *self, char *qstr, int first_doc,
                          int num_docs, FrtFilter *filter, Sort *sort,
                          PostFilter *post_filter)
{
    Query *query;
    TopDocs *td;
    query = index_get_query(self, qstr); /* will ensure_searcher is open */
    td = searcher_search(self->sea, query, first_doc, num_docs,
                         filter, sort, post_filter);
    q_deref(query);
    return td;
}

FrtDocument *index_get_doc(Index *self, int doc_num)
{
    FrtDocument *doc;
    ensure_reader_open(self);
    doc = self->ir->get_doc(self->ir, doc_num);
    return doc;
}

FrtDocument *index_get_doc_ts(Index *self, int doc_num)
{
    FrtDocument *doc;
    mutex_lock(&self->mutex);
    {
        doc = index_get_doc(self, doc_num);
    }
    mutex_unlock(&self->mutex);
    return doc;
}

int index_term_id(Index *self, Symbol field, const char *term)
{
    TermDocEnum *tde;
    int doc_num = -1;
    ensure_reader_open(self);
    tde = ir_term_docs_for(self->ir, field, term);
    if (tde->next(tde)) {
        doc_num = tde->doc_num(tde);
    }
    tde->close(tde);
    return doc_num;
}

FrtDocument *index_get_doc_term(Index *self, Symbol field,
                             const char *term)
{
    FrtDocument *doc = NULL;
    TermDocEnum *tde;
    mutex_lock(&self->mutex);
    {
        ensure_reader_open(self);
        tde = ir_term_docs_for(self->ir, field, term);
        if (tde->next(tde)) {
            doc = index_get_doc(self, tde->doc_num(tde));
        }
        tde->close(tde);
    }
    mutex_unlock(&self->mutex);
    return doc;
}

FrtDocument *index_get_doc_id(Index *self, const char *id)
{
    return index_get_doc_term(self, self->id_field, id);
}

void index_delete(Index *self, int doc_num)
{
    mutex_lock(&self->mutex);
    {
        ensure_reader_open(self);
        ir_delete_doc(self->ir, doc_num);
        AUTOFLUSH_IR(self);
    }
    mutex_unlock(&self->mutex);
}

void index_delete_term(Index *self, Symbol field, const char *term)
{
    TermDocEnum *tde;
    mutex_lock(&self->mutex);
    {
        if (self->ir) {
            tde = ir_term_docs_for(self->ir, field, term);
            FRT_TRY
                while (tde->next(tde)) {
                    ir_delete_doc(self->ir, tde->doc_num(tde));
                    AUTOFLUSH_IR(self);
                }
            FRT_XFINALLY
                tde->close(tde);
            FRT_XENDTRY
        } else {
            ensure_writer_open(self);
            iw_delete_term(self->iw, field, term);
        }
    }
    mutex_unlock(&self->mutex);
}

void index_delete_id(Index *self, const char *id)
{
    index_delete_term(self, self->id_field, id);
}

static void index_qdel_i(Searcher *sea, int doc_num, float score, void *arg)
{
    (void)score; (void)arg;
    ir_delete_doc(((IndexSearcher *)sea)->ir, doc_num);
}

void index_delete_query(Index *self, Query *q, FrtFilter *f,
                        PostFilter *post_filter)
{
    mutex_lock(&self->mutex);
    {
        ensure_searcher_open(self);
        searcher_search_each(self->sea, q, f, post_filter, &index_qdel_i, 0);
        AUTOFLUSH_IR(self);
    }
    mutex_unlock(&self->mutex);
}

void index_delete_query_str(Index *self, char *qstr, FrtFilter *f,
                            PostFilter *post_filter)
{
    Query *q = index_get_query(self, qstr);
    index_delete_query(self, q, f, post_filter);
    q_deref(q);
}

FrtExplanation *index_explain(Index *self, Query *q, int doc_num)
{
    FrtExplanation *expl;
    mutex_lock(&self->mutex);
    {
        ensure_searcher_open(self);
        expl = searcher_explain(self->sea, q, doc_num);
    }
    mutex_unlock(&self->mutex);
    return expl;
}
