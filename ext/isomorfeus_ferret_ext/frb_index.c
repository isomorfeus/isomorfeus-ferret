#include "frt_index.h"
#include "isomorfeus_ferret.h"

VALUE mIndex;

VALUE cFieldInfos;

VALUE cTVOffsets;
VALUE cTVTerm;
VALUE cTermVector;

VALUE cTermEnum;
VALUE cTermDocEnum;

VALUE cIndexWriter;
VALUE cIndexReader;

VALUE sym_analyzer;
VALUE sym_boost;

static VALUE sym_close_dir;
static VALUE sym_create;
static VALUE sym_create_if_missing;
static VALUE sym_chunk_size;
static VALUE sym_max_buffer_memory;
static VALUE sym_index_interval;
static VALUE sym_skip_interval;
static VALUE sym_merge_factor;
static VALUE sym_max_buffered_docs;
static VALUE sym_max_merge_docs;
static VALUE sym_max_field_length;
static VALUE sym_use_compound_file;
static VALUE sym_field_infos;

static ID fsym_content;
static ID id_term;
static ID id_fld_num_map;
static ID id_field_num;
static ID id_boost;

extern VALUE sym_each;
extern rb_encoding *utf8_encoding;
extern void frb_fi_get_params(VALUE roptions, unsigned int *bits, float *boost);
extern FrtAnalyzer *frb_get_cwrapped_analyzer(VALUE ranalyzer);
extern VALUE frb_get_analyzer(FrtAnalyzer *a);
extern VALUE frb_get_field_info(FrtFieldInfo *fi);
extern VALUE frb_get_lazy_doc(FrtLazyDoc *lazy_doc);
extern void frb_set_term(VALUE rterm, FrtTerm *t);

extern void Init_FieldInfo(void);
extern void Init_LazyDoc(void);

/****************************************************************************
 *
 * FieldInfos Methods
 *
 ****************************************************************************/

static void frb_fis_free(void *p) {
    frt_fis_deref((FrtFieldInfos *)p);
}

static void frb_fis_mark(void *p) {
    int i;
    FrtFieldInfos *fis = (FrtFieldInfos *)p;

    for (i = 0; i < fis->size; i++) {
        if (fis->fields[i]->rfi)
            rb_gc_mark(fis->fields[i]->rfi);
    }
}

static size_t frb_field_infos_t_size(const void *p) {
    return sizeof(FrtFieldInfos);
    (void)p;
}

const rb_data_type_t frb_field_infos_t = {
    .wrap_struct_name = "FrbFieldInfos",
    .function = {
        .dmark = frb_fis_mark,
        .dfree = frb_fis_free,
        .dsize = frb_field_infos_t_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_get_field_infos(FrtFieldInfos *fis) {
    if (fis) {
        if (fis->rfis == 0 || fis->rfis == Qnil) {
            fis->rfis = TypedData_Wrap_Struct(cFieldInfos, &frb_field_infos_t, fis);
            FRT_REF(fis);
        }
        return fis->rfis;
    }
    return Qnil;
}

/*
 *  call-seq:
 *     FieldInfos.new(defaults = {}) -> field_infos
 *
 *  Create a new FieldInfos object which uses the default values for fields
 *  specified in the +default+ hash parameter. See FieldInfo for available
 *  property values.
 */

static VALUE frb_fis_alloc(VALUE rclass) {
    FrtFieldInfos *fis = frt_fis_alloc();
    fis->size = 0;
    return TypedData_Wrap_Struct(rclass, &frb_field_infos_t, fis);
}

static VALUE frb_fis_init(int argc, VALUE *argv, VALUE self) {
    VALUE roptions;
    FrtFieldInfos *fis;
    TypedData_Get_Struct(self, FrtFieldInfos, &frb_field_infos_t, fis);
    unsigned int bits = FRT_FI_DEFAULTS_BM;
    float boost;

    rb_scan_args(argc, argv, "01", &roptions);
    if (argc > 0) frb_fi_get_params(roptions, &bits, &boost);
    fis = frt_fis_init(fis, bits);
    fis->rfis = self;
    return self;
}

/*
 *  call-seq:
 *     fis.to_a -> array
 *
 *  Return an array of the FieldInfo objects contained but this FieldInfos
 *  object.
 */
static VALUE frb_fis_to_a(VALUE self) {
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    VALUE rary = rb_ary_new();
    int i;

    for (i = 0; i < fis->size; i++) {
        rb_ary_push(rary, frb_get_field_info(fis->fields[i]));
    }
    return rary;
}

/*
 *  call-seq:
 *     fis[name] -> field_info
 *     fis[number] -> field_info
 *
 *  Get the FieldInfo object. FieldInfo objects can be referenced by either
 *  their field-number of the field-name (which must be a symbol). For
 *  example;
 *
 *    fi = fis[:name]
 *    fi = fis[2]
 */
static VALUE frb_fis_get(VALUE self, VALUE ridx) {
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    VALUE rfi = Qnil;
    switch (TYPE(ridx)) {
        case T_FIXNUM: {
            int index = FIX2INT(ridx);
            if (index < 0) index += fis->size;
            if (index < 0 || index >= fis->size) {
                rb_raise(rb_eArgError, "index of %d is out of range (0..%d)\n",
                         index, fis->size - 1);
            }
            rfi = frb_get_field_info(fis->fields[index]);
            break;
                       }
        case T_SYMBOL:
        case T_STRING:
            rfi = frb_get_field_info(frt_fis_get_field(fis, frb_field(ridx)));
            break;
        default:
            rb_raise(rb_eArgError, "Can't index FieldInfos with %s",
                     rs2s(rb_obj_as_string(ridx)));
            break;
    }
    return rfi;
}

/*
 *  call-seq:
 *     fis << fi -> fis
 *     fis.add(fi) -> fis
 *
 *  Add a FieldInfo object. Use the FieldInfos#add_field method where
 *  possible.
 */
static VALUE frb_fis_add(VALUE self, VALUE rfi) {
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    FrtFieldInfo *fi = (FrtFieldInfo *)frb_rb_data_ptr(rfi);
    frt_fis_add_field(fis, fi);
    FRT_REF(fi);
    return self;
}

/*
 *  call-seq:
 *     fis.add_field(name, properties = {} -> fis
 *
 *  Add a new field to the FieldInfos object. See FieldInfo for a description
 *  of the available properties.
 */
static VALUE
frb_fis_add_field(int argc, VALUE *argv, VALUE self)
{
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    FrtFieldInfo *fi;
    unsigned int bits = fis->bits;
    float boost = 1.0f;
    VALUE rname, roptions;

    rb_scan_args(argc, argv, "11", &rname, &roptions);
    if (argc > 1) {
        frb_fi_get_params(roptions, &bits, &boost);
    }
    fi = frt_fi_new(frb_field(rname), bits);
    fi->boost = boost;
    frt_fis_add_field(fis, fi);
    return self;
}

/*
 *  call-seq:
 *     fis.each {|fi| do_something } -> fis
 *
 *  Iterate through the FieldInfo objects.
 */
static VALUE
frb_fis_each(VALUE self)
{
    int i;
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);

    for (i = 0; i < fis->size; i++) {
        rb_yield(frb_get_field_info(fis->fields[i]));
    }
    return self;
}

/*
 *  call-seq:
 *     fis.to_s -> string
 *
 *  Return a string representation of the FieldInfos object.
 */
static VALUE
frb_fis_to_s(VALUE self)
{
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    char *fis_s = frt_fis_to_s(fis);
    VALUE rfis_s = rb_str_new2(fis_s);
    free(fis_s);
    return rfis_s;
}

/*
 *  call-seq:
 *     fis.size -> int
 *
 *  Return the number of fields in the FieldInfos object.
 */
static VALUE frb_fis_size(VALUE self) {
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    return INT2FIX(fis->size);
}

/*
 *  call-seq:
 *     fis.create_index(dir) -> self
 *
 *  Create a new index in the directory specified. The directory +dir+ can
 *  either be a string path representing a directory on the file-system or an
 *  actual directory object. Care should be taken when using this method. Any
 *  existing index (or other files for that matter) will be deleted from the
 *  directory and overwritten by the new index.
 */
static VALUE frb_fis_create_index(VALUE self, VALUE rdir) {
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    FrtStore *store = NULL;
    if (TYPE(rdir) == T_DATA) {
        store = DATA_PTR(rdir);
        frt_index_create(store, fis);
    } else {
        StringValue(rdir);
        frb_create_dir(rdir);
        store = frt_open_fs_store(rs2s(rdir));
        frt_index_create(store, fis);
        frt_store_close(store);
    }
    return self;
}

/*
 *  call-seq:
 *     fis.fields -> symbol array
 *     fis.field_names -> symbol array
 *
 *  Return a list of the field names (as symbols) of all the fields in the
 *  index.
 */
static VALUE
frb_fis_get_fields(VALUE self)
{
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    VALUE rfield_names = rb_ary_new();
    int i;
    for (i = 0; i < fis->size; i++) {
        rb_ary_push(rfield_names, ID2SYM(fis->fields[i]->name));
    }
    return rfield_names;
}

/*
 *  call-seq:
 *     fis.tokenized_fields -> symbol array
 *
 *  Return a list of the field names (as symbols) of all the tokenized fields
 *  in the index.
 */
static VALUE
frb_fis_get_tk_fields(VALUE self)
{
    FrtFieldInfos *fis = (FrtFieldInfos *)DATA_PTR(self);
    VALUE rfield_names = rb_ary_new();
    int i;
    for (i = 0; i < fis->size; i++) {
        if (!bits_is_tokenized(fis->fields[i]->bits)) continue;
        rb_ary_push(rfield_names, ID2SYM(fis->fields[i]->name));
    }
    return rfield_names;
}

/****************************************************************************
 *
 * TermEnum Methods
 *
 ****************************************************************************/

static void frb_te_free(void *p) {
    ((FrtTermEnum *)p)->close((FrtTermEnum *)p);
}

static size_t frb_te_size(const void *p) {
    return sizeof(FrtTermEnum);
    (void)p;
}

const rb_data_type_t frb_term_enum_t = {
    .wrap_struct_name = "FrbTermEnum",
    .function = {
        .dmark = NULL,
        .dfree = frb_te_free,
        .dsize = frb_te_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_te_alloc(VALUE rclass) {
    FrtTermEnum *te = FRT_ALLOC_AND_ZERO(FrtTermEnum);
    return TypedData_Wrap_Struct(rclass, &frb_term_enum_t, te);
}

static VALUE frb_te_get_set_term(VALUE self, const char *term) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    VALUE str = term ? rb_str_new(term, te->curr_term_len) : Qnil;
    rb_ivar_set(self, id_term, str);
    return str;
}

static VALUE frb_get_te(VALUE rir, FrtTermEnum *te) {
    VALUE self = Qnil;
    if (te != NULL) {
        self = TypedData_Wrap_Struct(cTermEnum, &frb_term_enum_t, te);
        frb_te_get_set_term(self, te->curr_term);
        rb_ivar_set(self, id_fld_num_map, rb_ivar_get(rir, id_fld_num_map));
    }
    return self;
}

/*
 *  call-seq:
 *     term_enum.next -> term_string
 *
 *  Returns the next term in the enumeration or nil otherwise.
 */
static VALUE frb_te_next(VALUE self) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    return frb_te_get_set_term(self, te->next(te));
}

/*
 *  call-seq:
 *     term_enum.term -> term_string
 *
 *  Returns the current term pointed to by the enum. This method should only
 *  be called after a successful call to TermEnum#next.
 */
static VALUE frb_te_term(VALUE self) {
    return rb_ivar_get(self, id_term);
}

/*
 *  call-seq:
 *     term_enum.doc_freq -> integer
 *
 *  Returns the document frequency of the current term pointed to by the enum.
 *  That is the number of documents that this term appears in. The method
 *  should only be called after a successful call to TermEnum#next.
 */
static VALUE frb_te_doc_freq(VALUE self) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    return INT2FIX(te->curr_ti.doc_freq);
}

/*
 *  call-seq:
 *     term_enum.skip_to(target) -> term
 *
 *  Skip to term +target+. This method can skip forwards or backwards. If you
 *  want to skip back to the start, pass the empty string "". That is;
 *
 *    term_enum.skip_to("")
 *
 *  Returns the first term greater than or equal to +target+
 */
static VALUE frb_te_skip_to(VALUE self, VALUE rterm) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    return frb_te_get_set_term(self, te->skip_to(te, rs2s(rterm)));
}

/*
 *  call-seq:
 *     term_enum.each {|term, doc_freq| do_something() } -> term_count
 *
 *  Iterates through all the terms in the field, yielding the term and the
 *  document frequency.
 */
static VALUE frb_te_each(VALUE self) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    char *term;
    int term_cnt = 0;
    VALUE vals = rb_ary_new2(2);
    rb_ary_store(vals, 0, Qnil);
    rb_ary_store(vals, 1, Qnil);

    /* each is being called so there will be no current term */
    rb_ivar_set(self, id_term, Qnil);


    while (NULL != (term = te->next(te))) {
        term_cnt++;
        RARRAY_PTR(vals)[0] = rb_str_new(term, te->curr_term_len);
        RARRAY_PTR(vals)[1] = INT2FIX(te->curr_ti.doc_freq);
        rb_yield(vals);
    }
    return INT2FIX(term_cnt);
}

/*
 *  call-seq:
 *     term_enum.set_field(field) -> self
 *
 *  Set the field for the term_enum. The field value should be a symbol as
 *  usual. For example, to scan all title terms you'd do this;
 *
 *    term_enum.set_field(:title).each do |term, doc_freq|
 *      do_something()
 *    end
 */
static VALUE frb_te_set_field(VALUE self, VALUE rfield) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    int field_num = 0;
    VALUE rfnum_map = rb_ivar_get(self, id_fld_num_map);
    VALUE rfnum = rb_hash_aref(rfnum_map, rfield);
    if (rfnum != Qnil) {
        field_num = FIX2INT(rfnum);
        rb_ivar_set(self, id_field_num, rfnum);
    } else {
        Check_Type(rfield, T_SYMBOL);
        rb_raise(rb_eArgError, "field %s doesn't exist in the index",
                rb_id2name(frb_field(rfield)));
    }
    te->set_field(te, field_num);

    return self;
}

/*
 *  call-seq:
 *     term_enum.to_json() -> string
 *
 *  Returns a JSON representation of the term enum. You can speed this up by
 *  having the method return arrays instead of objects, simply by passing an
 *  argument to the to_json method. For example;
 *
 *    term_enum.to_json() #=>
 *    # [
 *    #   {"term":"apple","frequency":12},
 *    #   {"term":"banana","frequency":2},
 *    #   {"term":"cantaloupe","frequency":12}
 *    # ]
 *
 *    term_enum.to_json(:fast) #=>
 *    # [
 *    #   ["apple",12],
 *    #   ["banana",2],
 *    #   ["cantaloupe",12]
 *    # ]
 */
static VALUE frb_te_to_json(int argc, VALUE *argv, VALUE self) {
    FrtTermEnum *te = (FrtTermEnum *)DATA_PTR(self);
    VALUE rjson;
    char *json, *jp;
    char *term;
    int capa = 65536;
    jp = json = FRT_ALLOC_N(char, capa);
    *(jp++) = '[';

    if (argc > 0) {
        while (NULL != (term = te->next(te))) {
            /* enough room for for term after converting " to '"' and frequency
             * plus some extra for good measure */
            *(jp++) = '[';
            if (te->curr_term_len * 3 + (jp - json) + 100 > capa) {
                capa <<= 1;
                FRT_REALLOC_N(json, char, capa);
            }
            jp = json_concat_string(jp, term);
            *(jp++) = ',';
            sprintf(jp, "%d", te->curr_ti.doc_freq);
            jp += strlen(jp);
            *(jp++) = ']';
            *(jp++) = ',';
        }
    } else {
        while (NULL != (term = te->next(te))) {
            /* enough room for for term after converting " to '"' and frequency
             * plus some extra for good measure */
            if (te->curr_term_len * 3 + (jp - json) + 100 > capa) {
                capa <<= 1;
                FRT_REALLOC_N(json, char, capa);
            }
            *(jp++) = '{';
            memcpy(jp, "\"term\":", 7);
            jp += 7;
            jp = json_concat_string(jp, term);
            *(jp++) = ',';
            memcpy(jp, "\"frequency\":", 12);
            jp += 12;
            sprintf(jp, "%d", te->curr_ti.doc_freq);
            jp += strlen(jp);
            *(jp++) = '}';
            *(jp++) = ',';
        }
    }
    if (*(jp-1) == ',') jp--;
    *(jp++) = ']';
    *jp = '\0';

    rjson = rb_str_new2(json);
    free(json);
    return rjson;
}

/****************************************************************************
 *
 * TermDocEnum Methods
 *
 ****************************************************************************/

static void frb_tde_free(void *p) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)p;
    tde->close(tde);
}

static size_t frb_tde_size(const void *p) {
    return sizeof(FrtTermDocEnum);
    (void)p;
}

const rb_data_type_t frb_term_doc_enum_t = {
    .wrap_struct_name = "FrbTermDocEnum",
    .function = {
        .dmark = NULL,
        .dfree = frb_tde_free,
        .dsize = frb_tde_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_tde_alloc(VALUE rclass) {
    FrtTermDocEnum *tde = FRT_ALLOC_AND_ZERO(FrtTermDocEnum);
    return TypedData_Wrap_Struct(rclass, &frb_term_doc_enum_t, tde);
}

static VALUE frb_get_tde(VALUE rir, FrtTermDocEnum *tde) {
    VALUE self = TypedData_Wrap_Struct(cTermDocEnum, &frb_term_doc_enum_t, tde);
    rb_ivar_set(self, id_fld_num_map, rb_ivar_get(rir, id_fld_num_map));
    return self;
}

/*
 *  call-seq:
 *     term_doc_enum.seek(field, term) -> self
 *
 *  Seek the term +term+ in the index for +field+. After you call this method
 *  you can call next or each to skip through the documents and positions of
 *  this particular term.
 */
static VALUE frb_tde_seek(VALUE self, VALUE rfield, VALUE rterm) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    char *term;
    VALUE rfnum_map = rb_ivar_get(self, id_fld_num_map);
    VALUE rfnum = rb_hash_aref(rfnum_map, rfield);
    int field_num = -1;
    term = StringValuePtr(rterm);
    if (rfnum != Qnil) {
        field_num = FIX2INT(rfnum);
    } else {
        rb_raise(rb_eArgError, "field %s doesn't exist in the index", rb_id2name(frb_field(rfield)));
    }
    tde->seek(tde, field_num, term);
    return self;
}

/*
 *  call-seq:
 *     term_doc_enum.seek_term_enum(term_enum) -> self
 *
 *  Seek the current term in +term_enum+. You could just use the standard seek
 *  method like this;
 *
 *    term_doc_enum.seek(term_enum.term)
 *
 *  However the +seek_term_enum+ method saves an index lookup so should offer
 *  a large performance improvement.
 */
static VALUE frb_tde_seek_te(VALUE self, VALUE rterm_enum) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    FrtTermEnum *te = (FrtTermEnum *)frb_rb_data_ptr(rterm_enum);
    tde->seek_te(tde, te);
    return self;
}

/*
 *  call-seq:
 *     term_doc_enum.doc -> doc_id
 *
 *  Returns the current document number pointed to by the +term_doc_enum+.
 */
static VALUE frb_tde_doc(VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    return INT2FIX(tde->doc_num(tde));
}

/*
 *  call-seq:
 *     term_doc_enum.doc -> doc_id
 *
 *  Returns the frequency of the current document pointed to by the
 *  +term_doc_enum+.
 */
static VALUE frb_tde_freq(VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    return INT2FIX(tde->freq(tde));
}

/*
 *  call-seq:
 *     term_doc_enum.doc -> doc_id
 *
 *  Move forward to the next document in the enumeration. Returns +true+ if
 *  there is another document or +false+ otherwise.
 */
static VALUE frb_tde_next(VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    return tde->next(tde) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     term_doc_enum.doc -> doc_id
 *
 *  Move forward to the next document in the enumeration. Returns +true+ if
 *  there is another document or +false+ otherwise.
 */
static VALUE frb_tde_next_position(VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    int pos;
    if (tde->next_position == NULL) {
        rb_raise(rb_eNotImpError, "to scan through positions you must create "
                 "the TermDocEnum with Index#term_positions method rather "
                 "than the Index#term_docs method");
    }
    pos = tde->next_position(tde);
    return pos >= 0 ? INT2FIX(pos) : Qnil;
}

/*
 *  call-seq:
 *     term_doc_enum.each {|doc_id, freq| do_something() } -> doc_count
 *
 *  Iterate through the documents and document frequencies in the
 *  +term_doc_enum+.
 *
 *  NOTE: this method can only be called once after each seek. If you need to
 *  call +#each+ again then you should call +#seek+ again too.
 */
static VALUE frb_tde_each(VALUE self) {
    int doc_cnt = 0;
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    VALUE vals = rb_ary_new2(2);
    rb_ary_store(vals, 0, Qnil);
    rb_ary_store(vals, 1, Qnil);

    while (tde->next(tde)) {
        doc_cnt++;
        RARRAY_PTR(vals)[0] = INT2FIX(tde->doc_num(tde));
        RARRAY_PTR(vals)[1] = INT2FIX(tde->freq(tde));
        rb_yield(vals);

    }
    return INT2FIX(doc_cnt);
}

/*
 *  call-seq:
 *     term_doc_enum.to_json() -> string
 *
 *  Returns a json representation of the term doc enum. It will also add the
 *  term positions if they are available. You can speed this up by having the
 *  method return arrays instead of objects, simply by passing an argument to
 *  the to_json method. For example;
 *
 *    term_doc_enum.to_json() #=>
 *    # [
 *    #   {"document":1,"frequency":12},
 *    #   {"document":11,"frequency":1},
 *    #   {"document":29,"frequency":120},
 *    #   {"document":30,"frequency":3}
 *    # ]
 *
 *    term_doc_enum.to_json(:fast) #=>
 *    # [
 *    #   [1,12],
 *    #   [11,1],
 *    #   [29,120],
 *    #   [30,3]
 *    # ]
 */
static VALUE frb_tde_to_json(int argc, VALUE *argv, VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    VALUE rjson;
    char *json, *jp;
    int capa = 65536;
    const char *format;
    char close = (argc > 0) ? ']' : '}';
    bool do_positions = tde->next_position != NULL;
    jp = json = FRT_ALLOC_N(char, capa);
    *(jp++) = '[';

    if (do_positions) {
        if (argc == 0) {
            format = "{\"document\":%d,\"frequency\":%d,\"positions\":[";
        }
        else {
            format = "[%d,%d,[";
        }
    }
    else {
        if (argc == 0) {
            format = "{\"document\":%d,\"frequency\":%d},";
        }
        else {
            format = "[%d,%d],";
        }
    }
    while (tde->next(tde)) {
        /* 100 chars should be enough room for an extra entry */
        if ((jp - json) + 100 + tde->freq(tde) * 20 > capa) {
            capa <<= 1;
            FRT_REALLOC_N(json, char, capa);
        }
        sprintf(jp, format, tde->doc_num(tde), tde->freq(tde));
        jp += strlen(jp);
        if (do_positions) {
            int pos;
            while (0 <= (pos = tde->next_position(tde))) {
                sprintf(jp, "%d,", pos);
                jp += strlen(jp);
            }
            if (*(jp - 1) == ',') jp--;
            *(jp++) = ']';
            *(jp++) = close;
            *(jp++) = ',';
        }
    }
    if (*(jp - 1) == ',') jp--;
    *(jp++) = ']';
    *jp = '\0';

    rjson = rb_str_new2(json);
    free(json);
    return rjson;
}

/*
 *  call-seq:
 *     term_doc_enum.each_position {|pos| do_something } -> term_doc_enum
 *
 *  Iterate through each of the positions occupied by the current term in the
 *  current document. This can only be called once per document. It can be
 *  used within the each method. For example, to print the terms documents and
 *  positions;
 *
 *    tde.each do |doc_id, freq|
 *      puts "term appeared #{freq} times in document #{doc_id}:"
 *      positions = []
 *      tde.each_position {|pos| positions << pos}
 *      puts "  #{positions.join(', ')}"
 *    end
 */
static VALUE frb_tde_each_position(VALUE self) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    int pos;
    if (tde->next_position == NULL) {
        rb_raise(rb_eNotImpError, "to scan through positions you must create "
                 "the TermDocEnum with Index#term_positions method rather "
                 "than the Index#term_docs method");
    }
    while (0 <= (pos = tde->next_position(tde))) {
        rb_yield(INT2FIX(pos));
    }
    return self;
}

/*
 *  call-seq:
 *     term_doc_enum.skip_to(target) -> bool
 *
 *  Skip to the required document number +target+ and return true if there is
 *  a document >= +target+.
 */
static VALUE frb_tde_skip_to(VALUE self, VALUE rtarget) {
    FrtTermDocEnum *tde = (FrtTermDocEnum *)DATA_PTR(self);
    return tde->skip_to(tde, FIX2INT(rtarget)) ? Qtrue : Qfalse;
}

/****************************************************************************
 *
 * TVOffsets Methods
 *
 ****************************************************************************/

static VALUE frb_get_tv_offsets(FrtOffset *offset) {
    return rb_struct_new(cTVOffsets,
                         ULL2NUM((frt_u64)offset->start),
                         ULL2NUM((frt_u64)offset->end),
                         NULL);
}

/****************************************************************************
 *
 * TVTerm Methods
 *
 ****************************************************************************/

static VALUE frb_get_tv_term(FrtTVTerm *tv_term) {
    int i;
    const int freq = tv_term->freq;
    VALUE rtext;
    VALUE rpositions = Qnil;
    rtext = rb_str_new2(tv_term->text);
    rb_enc_associate(rtext, utf8_encoding);
    if (tv_term->positions) {
        int *positions = tv_term->positions;
        rpositions = rb_ary_new2(freq);
        for (i = 0; i < freq; i++) {
          rb_ary_store(rpositions, i, INT2FIX(positions[i]));
        }
    }
    return rb_struct_new(cTVTerm, rtext, INT2FIX(freq), rpositions, NULL);
}

/****************************************************************************
 *
 * TermVector Methods
 *
 ****************************************************************************/

static VALUE frb_get_tv(FrtTermVector *tv) {
    int i;
    FrtTVTerm *terms = tv->terms;
    const int t_cnt = tv->term_cnt;
    const int o_cnt = tv->offset_cnt;
    VALUE rfield, rterms;
    VALUE roffsets = Qnil;
    rfield = ID2SYM(tv->field);

    rterms = rb_ary_new2(t_cnt);
    for (i = 0; i < t_cnt; i++) {
      rb_ary_store(rterms, i, frb_get_tv_term(&terms[i]));
    }

    if (tv->offsets) {
        FrtOffset *offsets = tv->offsets;
        roffsets = rb_ary_new2(o_cnt);
        for (i = 0; i < o_cnt; i++) {
          rb_ary_store(roffsets, i, frb_get_tv_offsets(&offsets[i]));
        }
    }

    return rb_struct_new(cTermVector, rfield, rterms, roffsets, NULL);
}

/****************************************************************************
 *
 * FrtIndexWriter Methods
 *
 ****************************************************************************/

void frb_iw_free(void *p) {
    frt_iw_close((FrtIndexWriter *)p);
}

void frb_iw_mark(void *p) {
    FrtIndexWriter *iw = (FrtIndexWriter *)p;
    if (iw->analyzer && iw->analyzer->ranalyzer)
        rb_gc_mark(iw->analyzer->ranalyzer);
    if (iw->store && iw->store->rstore)
        rb_gc_mark(iw->store->rstore);
    if (iw->fis && iw->fis->rfis)
        rb_gc_mark(iw->fis->rfis);
}

/*
 *  call-seq:
 *     index_writer.close -> nil
 *
 *  Close the IndexWriter. This will close and free all resources used
 *  exclusively by the index writer. The garbage collector will do this
 *  automatically if not called explicitly.
 */
static VALUE frb_iw_close(VALUE self) {
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    ((struct RData *)(self))->data = NULL;
    ((struct RData *)(self))->dmark = NULL;
    ((struct RData *)(self))->dfree = NULL;
    frt_iw_close(iw);
    return Qnil;
}

#define SET_INT_ATTR(attr) \
    do {\
        if (RTEST(rval = rb_hash_aref(roptions, sym_##attr)))\
            config.attr = FIX2INT(rval);\
    } while (0)

/*
 *  call-seq:
 *     IndexWriter.new(options = {}) -> index_writer
 *
 *  Create a new IndexWriter. You should either pass a path or a directory to
 *  this constructor. For example, here are three ways you can create an
 *  IndexWriter;
 *
 *    dir = RAMDirectory.new()
 *    iw = IndexWriter.new(:dir => dir)
 *
 *    dir = FSDirectory.new("/path/to/index")
 *    iw = IndexWriter.new(:dir => dir)
 *
 *    iw = IndexWriter.new(:path => "/path/to/index")
 *
 * See FrtIndexWriter for more options.
 */
static size_t frb_index_writer_t_size(const void *p) {
    return sizeof(FrtIndexWriter);
    (void)p;
}

const rb_data_type_t frb_index_writer_t = {
    .wrap_struct_name = "FrbIndexWriter",
    .function = {
        .dmark = frb_iw_mark,
        .dfree = frb_iw_free,
        .dsize = frb_index_writer_t_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_iw_alloc(VALUE rclass) {
    FrtIndexWriter *iw = frt_iw_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_index_writer_t, iw);
}

extern rb_data_type_t frb_store_t;

static VALUE frb_iw_init(int argc, VALUE *argv, VALUE self) {
    VALUE roptions, rval;
    bool create = false;
    bool create_if_missing = true;
    FrtStore *store = NULL;
    FrtAnalyzer *analyzer = NULL;
    FrtIndexWriter *volatile iw = NULL;
    FrtConfig config = frt_default_config;

    int ex_code = 0;
    const char *msg = NULL;

    rb_scan_args(argc, argv, "01", &roptions);
    FRT_TRY
        if (argc > 0) {
            Check_Type(roptions, T_HASH);

            if ((rval = rb_hash_aref(roptions, sym_dir)) != Qnil) {
                // Check_Type(rval, T_DATA);
                TypedData_Get_Struct(rval, FrtStore, &frb_store_t, store);
            } else if ((rval = rb_hash_aref(roptions, sym_path)) != Qnil) {
                StringValue(rval);
                frb_create_dir(rval);
                store = frt_open_fs_store(rs2s(rval));
            }
            /* use_compound_file defaults to true */
            config.use_compound_file =
                (rb_hash_aref(roptions, sym_use_compound_file) == Qfalse) ? false : true;

            if ((rval = rb_hash_aref(roptions, sym_analyzer)) != Qnil) {
                analyzer = frb_get_cwrapped_analyzer(rval);
            }

            create = RTEST(rb_hash_aref(roptions, sym_create));
            if ((rval = rb_hash_aref(roptions, sym_create_if_missing)) != Qnil) {
                create_if_missing = RTEST(rval);
            }
            SET_INT_ATTR(chunk_size);
            SET_INT_ATTR(max_buffer_memory);
            SET_INT_ATTR(index_interval);
            SET_INT_ATTR(skip_interval);
            SET_INT_ATTR(merge_factor);
            SET_INT_ATTR(max_buffered_docs);
            SET_INT_ATTR(max_merge_docs);
            SET_INT_ATTR(max_field_length);
        }
        if (NULL == store) {
            store = frt_open_ram_store(NULL);
        }
        if (!create && create_if_missing && !store->exists(store, "segments")) {
            create = true;
        }
        if (create) {
            FrtFieldInfos *fis;
            if ((rval = rb_hash_aref(roptions, sym_field_infos)) != Qnil) {
                TypedData_Get_Struct(rval, FrtFieldInfos, &frb_field_infos_t, fis);
                frt_index_create(store, fis);
            } else {
                fis = frt_fis_new(FRT_FI_DEFAULTS_BM);
                frt_index_create(store, fis);
                frt_fis_deref(fis);
            }
        }

        TypedData_Get_Struct(self, FrtIndexWriter, &frb_index_writer_t, iw);
        frt_iw_open(iw, store, analyzer, &config);
    FRT_XCATCHALL
        ex_code = xcontext.excode;
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY

    if (ex_code && msg) {
        ((struct RData *)(self))->data = NULL;
        ((struct RData *)(self))->dmark = NULL;
        ((struct RData *)(self))->dfree = NULL;
        frb_raise(ex_code, msg);
    }

    if (rb_block_given_p()) {
        rb_yield(self);
        frb_iw_close(self);
        return Qnil;
    } else {
        return self;
    }
}

/*
 *  call-seq:
 *     iw.doc_count -> number
 *
 *  Returns the number of documents in the Index. Note that deletions won't be
 *  taken into account until the FrtIndexWriter has been committed.
 */
static VALUE
frb_iw_get_doc_count(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(frt_iw_doc_count(iw));
}

static int
frb_hash_to_doc_i(VALUE key, VALUE value, VALUE arg)
{
    if (key == Qundef) {
        return ST_CONTINUE;
    } else {
        FrtDocument *doc = (FrtDocument *)arg;
        ID field = frb_field(key);
        VALUE val;
        FrtDocField *df;
        if (NULL == (df = frt_doc_get_field(doc, field))) {
            df = frt_df_new(field);
        }
        if (rb_respond_to(value, id_boost)) {
            df->boost = (float)NUM2DBL(rb_funcall(value, id_boost, 0));
        }
        switch (TYPE(value)) {
            case T_ARRAY:
                {
                    int i;
                    df->destroy_data = true;
                    for (i = 0; i < RARRAY_LEN(value); i++) {
                        val = rb_obj_as_string(RARRAY_PTR(value)[i]);
                        frt_df_add_data_len(df, rstrdup(val), RSTRING_LEN(val), rb_enc_get(val));
                    }
                }
                break;
            case T_STRING:
                frt_df_add_data_len(df, rs2s(value), RSTRING_LEN(value), rb_enc_get(value));
                break;
            default:
                val = rb_obj_as_string(value);
                df->destroy_data = true;
                frt_df_add_data_len(df, rstrdup(val), RSTRING_LEN(val), rb_enc_get(val));
                break;
        }
        frt_doc_add_field(doc, df);
    }
    return ST_CONTINUE;
}

static FrtDocument *
frb_get_doc(VALUE rdoc)
{
    VALUE val;
    FrtDocument *doc = frt_doc_new();
    FrtDocField *df;

    if (rb_respond_to(rdoc, id_boost)) {
        doc->boost = (float)NUM2DBL(rb_funcall(rdoc, id_boost, 0));
    }

    switch (TYPE(rdoc)) {
        case T_HASH:
            rb_hash_foreach(rdoc, frb_hash_to_doc_i, (VALUE)doc);
            break;
        case T_ARRAY:
            {
                int i;
                df = frt_df_new(fsym_content);
                df->destroy_data = true;
                for (i = 0; i < RARRAY_LEN(rdoc); i++) {
                    val = rb_obj_as_string(RARRAY_PTR(rdoc)[i]);
                    frt_df_add_data_len(df, rstrdup(val), RSTRING_LEN(val), rb_enc_get(val));
                }
                frt_doc_add_field(doc, df);
            }
            break;
        case T_SYMBOL:
            /* TODO: clean up this ugly cast */
            df = frt_df_add_data(frt_df_new(fsym_content), (char *)rb_id2name(SYM2ID(rdoc)), rb_enc_get(rdoc));
            frt_doc_add_field(doc, df);
            break;
        case T_STRING:
            df = frt_df_add_data_len(frt_df_new(fsym_content), rs2s(rdoc), RSTRING_LEN(rdoc), rb_enc_get(rdoc));
            frt_doc_add_field(doc, df);
            break;
        default:
            val = rb_obj_as_string(rdoc);
            df = frt_df_add_data_len(frt_df_new(fsym_content), rstrdup(val), RSTRING_LEN(val), rb_enc_get(val));
            df->destroy_data = true;
            frt_doc_add_field(doc, df);
            break;
    }
    return doc;
}

/*
 *  call-seq:
 *     iw << document -> iw
 *     iw.add_document(document) -> iw
 *
 *  Add a document to the index. See Document. A document can also be a simple
 *  hash object.
 */
static VALUE
frb_iw_add_doc(VALUE self, VALUE rdoc)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    FrtDocument *doc = frb_get_doc(rdoc);
    frt_iw_add_doc(iw, doc);
    frt_doc_destroy(doc);
    return self;
}

/*
 *  call-seq:
 *     iw.optimize -> iw
 *
 *  Optimize the index for searching. This commits any unwritten data to the
 *  index and optimizes the index into a single segment to improve search
 *  performance. This is an expensive operation and should not be called too
 *  often. The best time to call this is at the end of a long batch indexing
 *  process. Note that calling the optimize method do not in any way effect
 *  indexing speed (except for the time taken to complete the optimization
 *  process).
 */
static VALUE
frb_iw_optimize(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    frt_iw_optimize(iw);
    return self;
}

/*
 *  call-seq:
 *     iw.commit -> iw
 *
 *  Explicitly commit any changes to the index that may be hanging around in
 *  memory. You should call this method if you want to read the latest index
 *  with an IndexWriter.
 */
static VALUE
frb_iw_commit(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    frt_iw_commit(iw);
    return self;
}

/* index reader intermission */
static VALUE frb_ir_close(VALUE self);

void frb_ir_free(void *p) {
    frt_ir_close((FrtIndexReader *)p);
}

void frb_ir_mark(void *p) {
    FrtIndexReader *ir = (FrtIndexReader *)p;

    if (ir->type == FRT_MULTI_READER) {
        FrtMultiReader *mr = (FrtMultiReader *)p;
        int i;
        for (i = 0; i < mr->r_cnt; i++) {
            if (mr->sub_readers[i]->rir)
                rb_gc_mark(mr->sub_readers[i]->rir);
        }
    } else {
        if (ir->store && ir->store->rstore)
            rb_gc_mark(ir->store->rstore);
    }
}

static size_t frb_index_reader_t_size(const void *p) {
    return sizeof(FrtMultiReader);
    (void)p;
}

const rb_data_type_t frb_index_reader_t = {
    .wrap_struct_name = "FrbIndexReader",
    .function = {
        .dmark = frb_ir_mark,
        .dfree = frb_ir_free,
        .dsize = frb_index_reader_t_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

/*
 *  call-seq:
 *     iw.add_readers(reader_array) -> iw
 *
 *  Use this method to merge other indexes into the one being written by
 *  IndexWriter. This is useful for parallel indexing. You can have several
 *  indexing processes running in parallel, possibly even on different
 *  machines. Then you can finish by merging all of the indexes into a single
 *  index.
 */
static VALUE frb_iw_add_readers(VALUE self, VALUE rreaders) {
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    int i;
    FrtIndexReader **irs;
    Check_Type(rreaders, T_ARRAY);

    irs = FRT_ALLOC_N(FrtIndexReader *, RARRAY_LEN(rreaders));
    i = RARRAY_LEN(rreaders);
    while (i-- > 0) {
        FrtIndexReader *ir;
        TypedData_Get_Struct(RARRAY_PTR(rreaders)[i], FrtIndexReader, &frb_index_reader_t, ir);
        FRT_REF(ir);
        irs[i] = ir;
    }
    frt_iw_add_readers(iw, irs, RARRAY_LEN(rreaders));
    free(irs);
    return self;
}

/*
 *  call-seq:
 *     iw.delete(field, term)  -> iw
 *     iw.delete(field, terms) -> iw
 *
 *  Delete all documents in the index with the given +term+ or +terms+ in the
 *  field +field+. You should usually have a unique document id which you use
 *  with this method, rather then deleting all documents with the word "the"
 *  in them. There are of course exceptions to this rule. For example, you may
 *  want to delete all documents with the term "viagra" when deleting spam.
 */
static VALUE
frb_iw_delete(VALUE self, VALUE rfield, VALUE rterm)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    if (TYPE(rterm) == T_ARRAY) {
        const int term_cnt = RARRAY_LEN(rterm);
        int i;
        char **terms = FRT_ALLOC_N(char *, term_cnt);
        for (i = 0; i < term_cnt; i++) {
            terms[i] = StringValuePtr(RARRAY_PTR(rterm)[i]);
        }
        frt_iw_delete_terms(iw, frb_field(rfield), terms, term_cnt);
        free(terms);
    } else {
        frt_iw_delete_term(iw, frb_field(rfield), StringValuePtr(rterm));
    }
    return self;
}

/*
 *  call-seq:
 *     index_writer.field_infos -> FieldInfos
 *
 *  Get the FieldInfos object for this FrtIndexWriter. This is useful if you need
 *  to dynamically add new fields to the index with specific properties.
 */
static VALUE frb_iw_field_infos(VALUE self) {
    FrtIndexWriter *iw;
    TypedData_Get_Struct(self, FrtIndexWriter, &frb_index_writer_t, iw);
    return frb_get_field_infos(iw->fis);
}

/*
 *  call-seq:
 *     index_writer.analyzer -> FrtAnalyzer
 *
 *  Get the FrtAnalyzer for this IndexWriter. This is useful if you need
 *  to use the same analyzer in a QueryParser.
 */
static VALUE
frb_iw_get_analyzer(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return frb_get_analyzer(iw->analyzer);
}

/*
 *  call-seq:
 *     index_writer.analyzer -> FrtAnalyzer
 *
 *  Set the FrtAnalyzer for this IndexWriter. This is useful if you need to
 *  change the analyzer for a special document. It is risky though as the
 *  same analyzer will be used for all documents during search.
 */
static VALUE
frb_iw_set_analyzer(VALUE self, VALUE ranalyzer)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);

    frt_a_deref(iw->analyzer);
    iw->analyzer = frb_get_cwrapped_analyzer(ranalyzer);
    return ranalyzer;
}

/*
 *  call-seq:
 *     index_writer.version -> int
 *
 *  Returns the current version of the index writer.
 */
static VALUE
frb_iw_version(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return ULL2NUM(iw->sis->version);
}

/*
 *  call-seq:
 *     iw.chunk_size -> number
 *
 *  Return the current value of chunk_size
 */
static VALUE
frb_iw_get_chunk_size(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.chunk_size);
}

/*
 *  call-seq:
 *     iw.chunk_size = chunk_size -> chunk_size
 *
 *  Set the chunk_size parameter
 */
static VALUE
frb_iw_set_chunk_size(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.chunk_size = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.max_buffer_memory -> number
 *
 *  Return the current value of max_buffer_memory
 */
static VALUE
frb_iw_get_max_buffer_memory(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.max_buffer_memory);
}

/*
 *  call-seq:
 *     iw.max_buffer_memory = max_buffer_memory -> max_buffer_memory
 *
 *  Set the max_buffer_memory parameter
 */
static VALUE
frb_iw_set_max_buffer_memory(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.max_buffer_memory = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.term_index_interval -> number
 *
 *  Return the current value of term_index_interval
 */
static VALUE
frb_iw_get_index_interval(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.index_interval);
}

/*
 *  call-seq:
 *     iw.term_index_interval = term_index_interval -> term_index_interval
 *
 *  Set the term_index_interval parameter
 */
static VALUE
frb_iw_set_index_interval(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.index_interval = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.doc_skip_interval -> number
 *
 *  Return the current value of doc_skip_interval
 */
static VALUE
frb_iw_get_skip_interval(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.skip_interval);
}

/*
 *  call-seq:
 *     iw.doc_skip_interval = doc_skip_interval -> doc_skip_interval
 *
 *  Set the doc_skip_interval parameter
 */
static VALUE
frb_iw_set_skip_interval(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.skip_interval = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.merge_factor -> number
 *
 *  Return the current value of merge_factor
 */
static VALUE
frb_iw_get_merge_factor(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.merge_factor);
}

/*
 *  call-seq:
 *     iw.merge_factor = merge_factor -> merge_factor
 *
 *  Set the merge_factor parameter
 */
static VALUE
frb_iw_set_merge_factor(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.merge_factor = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.max_buffered_docs -> number
 *
 *  Return the current value of max_buffered_docs
 */
static VALUE
frb_iw_get_max_buffered_docs(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.max_buffered_docs);
}

/*
 *  call-seq:
 *     iw.max_buffered_docs = max_buffered_docs -> max_buffered_docs
 *
 *  Set the max_buffered_docs parameter
 */
static VALUE
frb_iw_set_max_buffered_docs(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.max_buffered_docs = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.max_merge_docs -> number
 *
 *  Return the current value of max_merge_docs
 */
static VALUE
frb_iw_get_max_merge_docs(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.max_merge_docs);
}

/*
 *  call-seq:
 *     iw.max_merge_docs = max_merge_docs -> max_merge_docs
 *
 *  Set the max_merge_docs parameter
 */
static VALUE
frb_iw_set_max_merge_docs(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.max_merge_docs = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.max_field_length -> number
 *
 *  Return the current value of max_field_length
 */
static VALUE
frb_iw_get_max_field_length(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return INT2FIX(iw->config.max_field_length);
}

/*
 *  call-seq:
 *     iw.max_field_length = max_field_length -> max_field_length
 *
 *  Set the max_field_length parameter
 */
static VALUE
frb_iw_set_max_field_length(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.max_field_length = FIX2INT(rval);
    return rval;
}

/*
 *  call-seq:
 *     iw.use_compound_file -> number
 *
 *  Return the current value of use_compound_file
 */
static VALUE
frb_iw_get_use_compound_file(VALUE self)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    return iw->config.use_compound_file ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     iw.use_compound_file = use_compound_file -> use_compound_file
 *
 *  Set the use_compound_file parameter
 */
static VALUE
frb_iw_set_use_compound_file(VALUE self, VALUE rval)
{
    FrtIndexWriter *iw = (FrtIndexWriter *)DATA_PTR(self);
    iw->config.use_compound_file = RTEST(rval);
    return rval;
}

/****************************************************************************
 *
 * IndexReader Methods
 *
 ****************************************************************************/

/*
 *  call-seq:
 *     IndexReader.new(dir) -> index_reader
 *
 *  Create a new IndexReader. You can either pass a string path to a
 *  file-system directory or an actual Ferret::Store::Directory object. For
 *  example;
 *
 *    dir = RAMDirectory.new()
 *    iw = IndexReader.new(dir)
 *
 *    dir = FSDirectory.new("/path/to/index")
 *    iw = IndexReader.new(dir)
 *
 *    iw = IndexReader.new("/path/to/index")
 *
 *  You can also create a what used to be known as a MultiReader by passing an
 *  array of IndexReader objects, Ferret::Store::Directory objects or
 *  file-system paths;
 *
 *    iw = IndexReader.new([dir, dir2, dir3])
 *
 *    iw = IndexReader.new([reader1, reader2, reader3])
 *
 *    iw = IndexReader.new(["/path/to/index1", "/path/to/index2"])
 */

static VALUE frb_ir_alloc(VALUE rclass) {
    // allocate for FrtSegmentReader, the largest of the Frt*Reader structs,
    // FrtIndexReader is part of it and later on its determined what its going to be
    FrtIndexReader *ir = (FrtIndexReader *)frt_sr_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_index_reader_t, ir);
}

static VALUE frb_ir_init(VALUE self, VALUE rdir) {
    FrtStore *store = NULL;
    FrtIndexReader *ir;
    int i;
    FrtFieldInfos *fis;
    VALUE rfield_num_map = rb_hash_new();
    int ex_code = 0;
    const char *msg = NULL;
    FRT_TRY
        if (TYPE(rdir) == T_ARRAY) {
            VALUE rdirs = rdir;
            const int reader_cnt = RARRAY_LEN(rdir);
            FrtIndexReader **sub_readers = FRT_ALLOC_N(FrtIndexReader *, reader_cnt);
            int i;
            for (i = 0; i < reader_cnt; i++) {
                rdir = RARRAY_PTR(rdirs)[i];
                switch (TYPE(rdir)) {
                    case T_DATA:
                        if (CLASS_OF(rdir) == cIndexReader) {
                            TypedData_Get_Struct(rdir, FrtIndexReader, &frb_index_reader_t, sub_readers[i]);
                            continue;
                        } else if (RTEST(rb_obj_is_kind_of(rdir, cDirectory))) {
                            store = DATA_PTR(rdir);
                        } else {
                            FRT_RAISE(FRT_ARG_ERROR, "A Multi-IndexReader can only "
                                    "be created from other IndexReaders, "
                                    "Directory objects or file-system paths. "
                                    "Not %s",
                                    rs2s(rb_obj_as_string(rdir)));
                        }
                        break;
                    case T_STRING:
                        frb_create_dir(rdir);
                        store = frt_open_fs_store(rs2s(rdir));
                        break;
                    default:
                        FRT_RAISE(FRT_ARG_ERROR, "%s isn't a valid directory "
                                "argument. You should use either a String or "
                                "a Directory",
                                rs2s(rb_obj_as_string(rdir)));
                        break;
                }
                sub_readers[i] = frt_ir_open(NULL, store);
                FRT_DEREF(sub_readers[i]);
            }
            TypedData_Get_Struct(self, FrtIndexReader, &frb_index_reader_t, ir);
            ir = frt_mr_open(ir, sub_readers, reader_cnt);
        } else {
            switch (TYPE(rdir)) {
                case T_DATA:
                    store = DATA_PTR(rdir);
                    break;
                case T_STRING:
                    frb_create_dir(rdir);
                    store = frt_open_fs_store(rs2s(rdir));
                    break;
                default:
                    FRT_RAISE(FRT_ARG_ERROR, "%s isn't a valid directory argument. "
                            "You should use either a String or a Directory",
                            rs2s(rb_obj_as_string(rdir)));
                    break;
            }
            TypedData_Get_Struct(self, FrtIndexReader, &frb_index_reader_t, ir);
            ir = frt_ir_open(ir, store);
        }
    FRT_XCATCHALL
        ex_code = xcontext.excode;
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY

    if (ex_code && msg) {
        ((struct RData *)(self))->data = NULL;
        ((struct RData *)(self))->dmark = NULL;
        ((struct RData *)(self))->dfree = NULL;
        frb_raise(ex_code, msg);
    }

    ir->rir = self;

    fis = ir->fis;
    for (i = 0; i < fis->size; i++) {
        FrtFieldInfo *fi = fis->fields[i];
        rb_hash_aset(rfield_num_map, ID2SYM(fi->name), INT2FIX(fi->number));
    }
    rb_ivar_set(self, id_fld_num_map, rfield_num_map);

    return self;
}

/*
 *  call-seq:
 *     index_reader.set_norm(doc_id, field, val)
 *
 *  Expert: change the boost value for a +field+ in document at +doc_id+.
 *  +val+ should be an integer in the range 0..255 which corresponds to an
 *  encoded float value.
 */
static VALUE
frb_ir_set_norm(VALUE self, VALUE rdoc_id, VALUE rfield, VALUE rval)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    frt_ir_set_norm(ir, FIX2INT(rdoc_id), frb_field(rfield), (frt_uchar)NUM2CHR(rval));
    return self;
}

/*
 *  call-seq:
 *     index_reader.norms(field) -> string
 *
 *  Expert: Returns a string containing the norm values for a field. The
 *  string length will be equal to the number of documents in the index and it
 *  could have null bytes.
 */
static VALUE
frb_ir_norms(VALUE self, VALUE rfield)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    frt_uchar *norms;
    norms = frt_ir_get_norms(ir, frb_field(rfield));
    if (norms) {
        return rb_str_new((char *)norms, ir->max_doc(ir));
    } else {
        return Qnil;
    }
}

/*
 *  call-seq:
 *     index_reader.get_norms_into(field, buffer, offset) -> buffer
 *
 *  Expert: Get the norm values into a string +buffer+ starting at +offset+.
 */
static VALUE
frb_ir_get_norms_into(VALUE self, VALUE rfield, VALUE rnorms, VALUE roffset)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    int offset;
    offset = FIX2INT(roffset);
    Check_Type(rnorms, T_STRING);
    if (RSTRING_LEN(rnorms) < offset + ir->max_doc(ir)) {
        rb_raise(rb_eArgError, "supplied a string of length:%ld to "
                 "IndexReader#get_norms_into but needed a string of length "
                 "offset:%d + maxdoc:%d",
                 RSTRING_LEN(rnorms), offset, ir->max_doc(ir));
    }

    frt_ir_get_norms_into(ir, frb_field(rfield),
                      (frt_uchar *)rs2s(rnorms) + offset);
    return rnorms;
}

/*
 *  call-seq:
 *     index_reader.commit -> index_reader
 *
 *  Commit any deletes made by this particular IndexReader to the index. This
 *  will use open a Commit lock.
 */
static VALUE
frb_ir_commit(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    frt_ir_commit(ir);
    return self;
}

/*
 *  call-seq:
 *     index_reader.close -> index_reader
 *
 *  Close the IndexReader. This method also commits any deletions made by this
 *  IndexReader. This method will be called explicitly by the garbage
 *  collector but you should call it explicitly to commit any changes as soon
 *  as possible and to close any locks held by the object to prevent locking
 *  errors.
 */
static VALUE
frb_ir_close(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    ((struct RData *)(self))->data = NULL;
    ((struct RData *)(self))->dmark = NULL;
    ((struct RData *)(self))->dfree = NULL;
    frt_ir_close(ir);
    return self;
}

/*
 *  call-seq:
 *     index_reader.has_deletions? -> bool
 *
 *  Return true if the index has any deletions, either uncommitted by this
 *  IndexReader or committed by any other IndexReader.
 */
static VALUE
frb_ir_has_deletions(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return ir->has_deletions(ir) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     index_reader.delete(doc_id) -> index_reader
 *
 *  Delete document referenced internally by document id +doc_id+. The
 *  document_id is the number used to reference documents in the index and is
 *  returned by search methods.
 */
static VALUE
frb_ir_delete(VALUE self, VALUE rdoc_id)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    frt_ir_delete_doc(ir, FIX2INT(rdoc_id));
    return self;
}

/*
 *  call-seq:
 *     index_reader.deleted?(doc_id) -> bool
 *
 *  Returns true if the document at +doc_id+ has been deleted.
 */
static VALUE
frb_ir_is_deleted(VALUE self, VALUE rdoc_id)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return ir->is_deleted(ir, FIX2INT(rdoc_id)) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     index_reader.max_doc -> number
 *
 *  Returns 1 + the maximum document id in the index. It is the
 *  document_id that will be used by the next document added to the index. If
 *  there are no deletions, this number also refers to the number of documents
 *  in the index.
 */
static VALUE
frb_ir_max_doc(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return INT2FIX(ir->max_doc(ir));
}

/*
 *  call-seq:
 *     index_reader.num_docs -> number
 *
 *  Returns the number of accessible (not deleted) documents in the index.
 *  This will be equal to IndexReader#max_doc if there have been no documents
 *  deleted from the index.
 */
static VALUE
frb_ir_num_docs(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return INT2FIX(ir->num_docs(ir));
}

/*
 *  call-seq:
 *     index_reader.undelete_all -> index_reader
 *
 *  Undelete all deleted documents in the index. This is kind of like a
 *  rollback feature. Not that once an index is committed or a merge happens
 *  during index, deletions will be committed and undelete_all will have no
 *  effect on these documents.
 */
static VALUE
frb_ir_undelete_all(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    frt_ir_undelete_all(ir);
    return self;
}

static VALUE
frb_get_doc_range(FrtIndexReader *ir, int pos, int len, int max)
{
    VALUE ary;
    int i;
    max = FRT_MIN(max, pos+len);
    len = max - pos;
    ary = rb_ary_new2(len);
    for (i = 0; i < len; i++) {
      rb_ary_store(ary, i, frb_get_lazy_doc(ir->get_lazy_doc(ir, i + pos)));
    }
    return ary;
}

/*
 *  call-seq:
 *     index_reader.get_document(doc_id) -> LazyDoc
 *     index_reader[doc_id] -> LazyDoc
 *
 *  Retrieve a document from the index. See LazyDoc for more details on the
 *  document returned. Documents are referenced internally by document ids
 *  which are returned by the Searchers search methods.
 */
static VALUE
frb_ir_get_doc(int argc, VALUE *argv, VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    VALUE arg1, arg2;
    long pos, len;
    long max = ir->max_doc(ir);
    rb_scan_args(argc, argv, "11", &arg1, &arg2);
    if (argc == 1) {
        if (FIXNUM_P(arg1)) {
            pos = FIX2INT(arg1);
            pos = (pos < 0) ? (max + pos) : pos;
            if (pos < 0 || pos >= max) {
                rb_raise(rb_eArgError, "index %ld is out of range [%d..%ld] for "
                         "IndexReader#[]", pos, 0, max);
            }
            return frb_get_lazy_doc(ir->get_lazy_doc(ir, pos));
        }

        /* check if idx is Range */
        /* FIXME: test this with dodgy values */
        switch (rb_range_beg_len(arg1, &pos, &len, max, 0)) {
            case Qfalse:
                rb_raise(rb_eArgError, ":%s isn't a valid argument for "
                         "IndexReader.get_document(index)",
                         rb_id2name(SYM2ID(arg1)));
            case Qnil:
                return Qnil;
            default:
                return frb_get_doc_range(ir, pos, len, max);
        }
    }
    else {
        pos = FIX2LONG(arg1);
        len = FIX2LONG(arg2);
        return frb_get_doc_range(ir, pos, len, max);
    }
}

/*
 *  call-seq:
 *     index_reader.is_latest? -> bool
 *
 *  Return true if the index version referenced by this IndexReader is the
 *  latest version of the index. If it isn't you should close and reopen the
 *  index to search the latest documents added to the index.
 */
static VALUE
frb_ir_is_latest(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frt_ir_is_latest(ir) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     index_reader.term_vector(doc_id, field) -> TermVector
 *
 *  Return the TermVector for the field +field+ in the document at +doc_id+ in
 *  the index. Return nil if no such term_vector exists. See TermVector.
 */
static VALUE
frb_ir_term_vector(VALUE self, VALUE rdoc_id, VALUE rfield)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    FrtTermVector *tv;
    VALUE rtv;
    tv = ir->term_vector(ir, FIX2INT(rdoc_id), frb_field(rfield));
    if (tv) {
        rtv = frb_get_tv(tv);
        frt_tv_destroy(tv);
        return rtv;
    }
    else {
        return Qnil;
    }
}

static void
frb_add_each_tv(void *key, void *value, void *rtvs)
{
    rb_hash_aset((VALUE)rtvs, ID2SYM((ID)key), frb_get_tv(value));
}

/*
 *  call-seq:
 *     index_reader.term_vectors(doc_id) -> hash of TermVector
 *
 *  Return the TermVectors for the document at +doc_id+ in the index. The
 *  value returned is a hash of the TermVectors for each field in the document
 *  and they are referenced by field names (as symbols).
 */
static VALUE
frb_ir_term_vectors(VALUE self, VALUE rdoc_id)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    FrtHash *tvs = ir->term_vectors(ir, FIX2INT(rdoc_id));
    VALUE rtvs = rb_hash_new();
    frt_h_each(tvs, &frb_add_each_tv, (void *)rtvs);
    frt_h_destroy(tvs);
    return rtvs;
}

/*
 *  call-seq:
 *     index_reader.term_docs -> TermDocEnum
 *
 *  Builds a TermDocEnum (term-document enumerator) for the index. You can use
 *  this object to iterate through the documents in which certain terms occur.
 *  See TermDocEnum for more info.
 */
static VALUE
frb_ir_term_docs(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_tde(self, ir->term_docs(ir));
}

/*
 *  call-seq:
 *     index_reader.term_docs_for(field, term) -> TermDocEnum
 *
 *  Builds a TermDocEnum to iterate through the documents that contain the
 *  term +term+ in the field +field+. See TermDocEnum for more info.
 */
static VALUE
frb_ir_term_docs_for(VALUE self, VALUE rfield, VALUE rterm)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_tde(self, ir_term_docs_for(ir,
                                              frb_field(rfield),
                                              StringValuePtr(rterm)));
}

/*
 *  call-seq:
 *     index_reader.term_positions -> TermDocEnum
 *
 *  Same as IndexReader#term_docs except the TermDocEnum will also allow you
 *  to scan through the positions at which a term occurs. See TermDocEnum for
 *  more info.
 */
static VALUE
frb_ir_term_positions(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_tde(self, ir->term_positions(ir));
}

/*
 *  call-seq:
 *     index_reader.term_positions_for(field, term) -> TermDocEnum
 *
 *  Same as IndexReader#term_docs_for(field, term) except the TermDocEnum will
 *  also allow you to scan through the positions at which a term occurs. See
 *  TermDocEnum for more info.
 */
static VALUE
frb_ir_t_pos_for(VALUE self, VALUE rfield, VALUE rterm)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_tde(self, frt_ir_term_positions_for(ir,
                                                   frb_field(rfield),
                                                   StringValuePtr(rterm)));
}

/*
 *  call-seq:
 *     index_reader.doc_freq(field, term) -> integer
 *
 *  Return the number of documents in which the term +term+ appears in the
 *  field +field+.
 */
static VALUE
frb_ir_doc_freq(VALUE self, VALUE rfield, VALUE rterm)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return INT2FIX(frt_ir_doc_freq(ir,
                               frb_field(rfield),
                               StringValuePtr(rterm)));
}

/*
 *  call-seq:
 *     index_reader.terms(field) -> TermEnum
 *
 *  Returns a term enumerator which allows you to iterate through all the
 *  terms in the field +field+ in the index.
 */
static VALUE
frb_ir_terms(VALUE self, VALUE rfield)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_te(self, frt_ir_terms(ir, frb_field(rfield)));
}

/*
 *  call-seq:
 *     index_reader.terms_from(field, term) -> TermEnum
 *
 *  Same as IndexReader#terms(fields) except that it starts the enumerator off
 *  at term +term+.
 */
static VALUE
frb_ir_terms_from(VALUE self, VALUE rfield, VALUE rterm)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return frb_get_te(self, frt_ir_terms_from(ir,
                                          frb_field(rfield),
                                          StringValuePtr(rterm)));
}

/*
 *  call-seq:
 *     index_reader.term_count(field) -> int
 *
 *  Same return a count of the number of terms in the field
 */
static VALUE
frb_ir_term_count(VALUE self, VALUE rfield)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    FrtTermEnum *te = frt_ir_terms(ir, frb_field(rfield));
    int count = 0;
    while (te->next(te)) {
        count++;
    }
    te->close(te);
    return INT2FIX(count);
}

/*
 *  call-seq:
 *     index_reader.fields -> array of field-names
 *
 *  Returns an array of field names in the index. This can be used to pass to
 *  the QueryParser so that the QueryParser knows how to expand the "*"
 *  wild-card to all fields in the index. A list of field names can also be
 *  gathered from the FieldInfos object.
 */
static VALUE
frb_ir_fields(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    FrtFieldInfos *fis = ir->fis;
    VALUE rfield_names = rb_ary_new();
    int i;
    for (i = 0; i < fis->size; i++) {
        rb_ary_push(rfield_names, ID2SYM(fis->fields[i]->name));
    }
    return rfield_names;
}

/*
 *  call-seq:
 *     index_reader.field_infos -> FieldInfos
 *
 *  Get the FieldInfos object for this IndexReader.
 */
static VALUE frb_ir_field_infos(VALUE self) {
    FrtIndexReader *ir;
    TypedData_Get_Struct(self, FrtIndexReader, &frb_index_reader_t, ir);
    return frb_get_field_infos(ir->fis);
}

/*
 *  call-seq:
 *     index_reader.tokenized_fields -> array of field-names
 *
 *  Returns an array of field names of all of the tokenized fields in the
 *  index. This can be used to pass to the QueryParser so that the QueryParser
 *  knows how to expand the "*" wild-card to all fields in the index. A list
 *  of field names can also be gathered from the FieldInfos object.
 */
static VALUE
frb_ir_tk_fields(VALUE self)
{
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    FrtFieldInfos *fis = ir->fis;
    VALUE rfield_names = rb_ary_new();
    int i;
    for (i = 0; i < fis->size; i++) {
        if (!bits_is_tokenized(fis->fields[i]->bits)) continue;
        rb_ary_push(rfield_names, rb_str_new_cstr(rb_id2name(fis->fields[i]->name)));
    }
    return rfield_names;
}

/*
 *  call-seq:
 *     index_reader.version -> int
 *
 *  Returns the current version of the index reader.
 */
static VALUE
frb_ir_version(VALUE self) {
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    return ULL2NUM(ir->sis->version);
}

static VALUE frb_ir_to_enum(VALUE self) {
    return rb_enumeratorize(self, sym_each, 0, NULL);
}

static VALUE frb_ir_each(VALUE self) {
    FrtIndexReader *ir = (FrtIndexReader *)DATA_PTR(self);
    if (rb_block_given_p()) {
        long i;
        long max_doc = ir->max_doc(ir);
        VALUE rld;
        for (i = 0; i < max_doc; i++) {
            if (ir->is_deleted(ir, i)) continue;
            rld = frb_get_lazy_doc(ir->get_lazy_doc(ir, i));
            rb_yield(rld);
        }
        return self;
    } else {
        return frb_ir_to_enum(self);
    }

}

/****************************************************************************
 *
 * Init Functions
 *
 ****************************************************************************/

/*
 *  Document-class: Ferret::Index::FieldInfos
 *
 *  == Summary
 *
 *  The FieldInfos class holds all the field descriptors for an index. It is
 *  this class that is used to create a new index using the
 *  FieldInfos#create_index method. If you are happy with the default
 *  properties for FieldInfo then you don't need to worry about this class.
 *  FrtIndexWriter can create the index for you. Otherwise you should set up the
 *  index like in the example;
 *
 *  == Example
 *
 *    field_infos = FieldInfos.new(:term_vector => :no)
 *
 *    field_infos.add_field(:title, :index => :untokenized, :term_vector => :no,
 *                          :boost => 10.0)
 *
 *    field_infos.add_field(:content)
 *
 *    field_infos.add_field(:created_on, :index => :untokenized_omit_norms,
 *                          :term_vector => :no)
 *
 *    field_infos.add_field(:image, :store => :yes, :compression => :brotli, :index => :no,
 *                          :term_vector => :no)
 *
 *    field_infos.create_index("/path/to/index")
 *
 *  == Default Properties
 *
 *  See FieldInfo for the available field property values.
 *
 *  When you create the FieldInfos object you specify the default properties
 *  for the fields. Often you'll specify all of the fields in the index before
 *  you create the index so the default values won't come into play. However,
 *  it is possible to continue to dynamically add fields as indexing goes
 *  along. If you add a document to the index which has fields that the index
 *  doesn't know about then the default properties are used for the new field.
 */
static void Init_FieldInfos(void) {
    Init_FieldInfo();

    cFieldInfos = rb_define_class_under(mIndex, "FieldInfos", rb_cObject);
    rb_define_alloc_func(cFieldInfos, frb_fis_alloc);

    rb_define_method(cFieldInfos, "initialize", frb_fis_init, -1);
    rb_define_method(cFieldInfos, "to_a",       frb_fis_to_a, 0);
    rb_define_method(cFieldInfos, "[]",         frb_fis_get, 1);
    rb_define_method(cFieldInfos, "add",        frb_fis_add, 1);
    rb_define_method(cFieldInfos, "<<",         frb_fis_add, 1);
    rb_define_method(cFieldInfos, "add_field",  frb_fis_add_field, -1);
    rb_define_method(cFieldInfos, "each",       frb_fis_each, 0);
    rb_define_method(cFieldInfos, "to_s",       frb_fis_to_s, 0);
    rb_define_method(cFieldInfos, "size",       frb_fis_size, 0);
    rb_define_method(cFieldInfos, "create_index",
                                                frb_fis_create_index, 1);
    rb_define_method(cFieldInfos, "fields",     frb_fis_get_fields, 0);
    rb_define_method(cFieldInfos, "tokenized_fields", frb_fis_get_tk_fields, 0);
}

/*
 *  Document-class: Ferret::Index::TermEnum
 *
 *  == Summary
 *
 *  The TermEnum object is used to iterate through the terms in a field. To
 *  get a TermEnum you need to use the IndexReader#terms(field) method.
 *
 *  == Example
 *
 *    te = index_reader.terms(:content)
 *
 *    te.each {|term, doc_freq| puts "#{term} occurred #{doc_freq} times" }
 *
 *    # or you could do it like this;
 *    te = index_reader.terms(:content)
 *
 *    while te.next?
 *      puts "#{te.term} occurred in #{te.doc_freq} documents in the index"
 *    end
 */
static void
Init_TermEnum(void) {
    id_term = rb_intern("@term");

    cTermEnum = rb_define_class_under(mIndex, "TermEnum", rb_cObject);
    rb_define_alloc_func(cTermEnum, frb_te_alloc);

    rb_define_method(cTermEnum, "next?",    frb_te_next, 0);
    rb_define_method(cTermEnum, "term",     frb_te_term, 0);
    rb_define_method(cTermEnum, "doc_freq", frb_te_doc_freq, 0);
    rb_define_method(cTermEnum, "skip_to",  frb_te_skip_to, 1);
    rb_define_method(cTermEnum, "each",     frb_te_each, 0);
    rb_define_method(cTermEnum, "field=",   frb_te_set_field, 1);
    rb_define_method(cTermEnum, "set_field",frb_te_set_field, 1);
    rb_define_method(cTermEnum, "to_json",  frb_te_to_json, -1);
}

/*
 *  Document-class: Ferret::Index::TermDocEnum
 *
 *  == Summary
 *
 *  Use a TermDocEnum to iterate through the documents that contain a
 *  particular term. You can also iterate through the positions which the term
 *  occurs in a document.
 *
 *
 *  == Example
 *
 *    tde = index_reader.term_docs_for(:content, "fox")
 *
 *    tde.each do |doc_id, freq|
 *      puts "fox appeared #{freq} times in document #{doc_id}:"
 *      positions = []
 *      tde.each_position {|pos| positions << pos}
 *      puts "  #{positions.join(', ')}"
 *    end
 *
 *    # or you can do it like this;
 *    tde.seek(:title, "red")
 *    while tde.next?
 *      puts "red appeared #{tde.freq} times in document #{tde.doc}:"
 *      positions = []
 *      while pos = tde.next_position
 *        positions << pos
 *      end
 *      puts "  #{positions.join(', ')}"
 *    end
 */
static void Init_TermDocEnum(void) {
    id_fld_num_map = rb_intern("@field_num_map");
    id_field_num = rb_intern("@field_num");

    cTermDocEnum = rb_define_class_under(mIndex, "TermDocEnum", rb_cObject);
    rb_define_alloc_func(cTermDocEnum, frb_tde_alloc);
    rb_define_method(cTermDocEnum, "seek",           frb_tde_seek, 2);
    rb_define_method(cTermDocEnum, "seek_term_enum", frb_tde_seek_te, 1);
    rb_define_method(cTermDocEnum, "doc",            frb_tde_doc, 0);
    rb_define_method(cTermDocEnum, "freq",           frb_tde_freq, 0);
    rb_define_method(cTermDocEnum, "next?",          frb_tde_next, 0);
    rb_define_method(cTermDocEnum, "next_position",  frb_tde_next_position, 0);
    rb_define_method(cTermDocEnum, "each",           frb_tde_each, 0);
    rb_define_method(cTermDocEnum, "each_position",  frb_tde_each_position, 0);
    rb_define_method(cTermDocEnum, "skip_to",        frb_tde_skip_to, 1);
    rb_define_method(cTermDocEnum, "to_json",        frb_tde_to_json, -1);
}

/*
 *  Document-class: Ferret::Index::TermVector::TVOffsets
 *
 *  == Summary
 *
 *  Holds the start and end byte-offsets of a term in a field. For example, if
 *  the field was "the quick brown fox" then the start and end offsets of:
 *
 *    ["the", "quick", "brown", "fox"]
 *
 *  Would be:
 *
 *    [(0,3), (4,9), (10,15), (16,19)]
 *
 *  See the Analysis module for more information on setting the offsets.
 */
static void Init_TVOffsets(void) {
    const char *tv_offsets_class = "TVOffsets";
    cTVOffsets = rb_struct_define(tv_offsets_class, "start", "end", NULL);
    rb_set_class_path(cTVOffsets, cTermVector, tv_offsets_class);
    rb_const_set(mIndex, rb_intern(tv_offsets_class), cTVOffsets);
}

/*
 *  Document-class: Ferret::Index::TermVector::TVTerm
 *
 *  == Summary
 *
 *  The TVTerm class holds the term information for each term in a TermVector.
 *  That is it holds the term's text and its positions in the document. You
 *  can use those positions to reference the offsets for the term.
 *
 *  == Example
 *
 *    tv = index_reader.term_vector(:content)
 *    tv_term = tv.find {|tvt| tvt.term = "fox"}
 *    offsets = tv_term.positions.collect {|pos| tv.offsets[pos]}
 */
static void Init_TVTerm(void) {
    const char *tv_term_class = "TVTerm";
    cTVTerm = rb_struct_define(tv_term_class, "text", "freq", "positions", NULL);
    rb_set_class_path(cTVTerm, cTermVector, tv_term_class);
    rb_const_set(mIndex, rb_intern(tv_term_class), cTVTerm);
}

/*
 *  Document-class: Ferret::Index::TermVector
 *
 *  == Summary
 *
 *  TermVectors are most commonly used for creating search result excerpts and
 *  highlight search matches in results. This is all done internally so you
 *  won't need to worry about the TermVector object. There are some other
 *  reasons you may want to use the TermVectors object however. For example,
 *  you may wish to see which terms are the most commonly occurring terms in a
 *  document to implement a MoreLikeThis search.
 *
 *  == Example
 *
 *    tv = index_reader.term_vector(doc_id, :content)
 *    tv_term = tv.find {|tvt| tvt.term == "fox"}
 *
 *    # get the term frequency
 *    term_freq = tv_term.positions.size
 *
 *    # get the offsets for a term
 *    offsets = tv_term.positions.collect {|pos| tv.offsets[pos]}
 *
 *  == Note
 *
 *  +positions+ and +offsets+ can be +nil+ depending on what you set the
 *  +:term_vector+ to when you set the FieldInfo object for the field. Note in
 *  particular that you need to store both positions and offsets if you want
 *  to associate offsets with particular terms.
 */
static void Init_TermVector(void) {
    const char *tv_class = "TermVector";
    cTermVector = rb_struct_define(tv_class, "field", "terms", "offsets", NULL);
    rb_set_class_path(cTermVector, mIndex, tv_class);
    rb_const_set(mIndex, rb_intern(tv_class), cTermVector);

    Init_TVOffsets();
    Init_TVTerm();
}

/*
 *  Document-class: Ferret::Index::IndexWriter
 *
 *  == Summary
 *
 *  The FrtIndexWriter is the class used to add documents to an index. You can
 *  also delete documents from the index using this class. The indexing
 *  process is highly customizable and the FrtIndexWriter has the following
 *  parameters;
 *
 *  dir::                 This is an Ferret::Store::Directory object. You
 *                        should either pass a +:dir+ or a +:path+ when
 *                        creating an index.
 *  path::                A string representing the path to the index
 *                        directory. If you are creating the index for the
 *                        first time the directory will be created if it's
 *                        missing. You should not choose a directory which
 *                        contains other files as they could be over-written.
 *                        To protect against this set +:create_if_missing+ to
 *                        false.
 *  create_if_missing::   Default: true. Create the index if no index is
 *                        found in the specified directory. Otherwise, use
 *                        the existing index.
 *  create::              Default: false. Creates the index, even if one
 *                        already exists.  That means any existing index will
 *                        be deleted. It is probably better to use the
 *                        create_if_missing option so that the index is only
 *                        created the first time when it doesn't exist.
 *  field_infos::         Default FieldInfos.new. The FieldInfos object to use
 *                        when creating a new index if +:create_if_missing+ or
 *                        +:create+ is set to true. If an existing index is
 *                        opened then this parameter is ignored.
 *  analyzer::            Default: Ferret::Analysis::StandardAnalyzer.
 *                        Sets the default analyzer for the index. This is
 *                        used by both the FrtIndexWriter and the QueryParser
 *                        to tokenize the input. The default is the
 *                        StandardAnalyzer.
 *  chunk_size::          Default: 0x100000 or 1Mb. Memory performance tuning
 *                        parameter. Sets the default size of chunks of memory
 *                        malloced for use during indexing. You can usually
 *                        leave this parameter as is.
 *  max_buffer_memory::   Default: 0x1000000 or 16Mb. Memory performance
 *                        tuning parameter. Sets the amount of memory to be
 *                        used by the indexing process. Set to a larger value
 *                        to increase indexing speed. Note that this only
 *                        includes memory used by the indexing process, not
 *                        the rest of your ruby application.
 *  term_index_interval:: Default: 128. The skip interval between terms in the
 *                        term dictionary. A smaller value will possibly
 *                        increase search performance while also increasing
 *                        memory usage and impacting negatively impacting
 *                        indexing performance.
 *  doc_skip_interval::   Default: 16. The skip interval for document numbers
 *                        in the index. As with +:term_index_interval+ you
 *                        have a trade-off. A smaller number may increase
 *                        search performance while also increasing memory
 *                        usage and impacting negatively impacting indexing
 *                        performance.
 *  merge_factor::        Default: 10. This must never be less than 2.
 *                        Specifies the number of segments of a certain size
 *                        that must exist before they are merged. A larger
 *                        value will improve indexing performance while
 *                        slowing search performance.
 *  max_buffered_docs::   Default: 10000. The maximum number of documents that
 *                        may be stored in memory before being written to the
 *                        index. If you have a lot of memory and are indexing
 *                        a large number of small documents (like products in
 *                        a product database for example) you may want to set
 *                        this to a much higher number (like
 *                        Ferret::FIX_INT_MAX). If you are worried about your
 *                        application crashing during the middle of index you
 *                        might set this to a smaller number so that the index
 *                        is committed more often. This is like having an
 *                        auto-save in a word processor application.
 *  max_merge_docs::      Set this value to limit the number of documents that
 *                        go into a single segment. Use this to avoid
 *                        extremely long merge times during indexing which can
 *                        make your application seem unresponsive. This is
 *                        only necessary for very large indexes (millions of
 *                        documents).
 *  max_field_length::    Default: 10000. The maximum number of terms added to
 *                        a single field.  This can be useful to protect the
 *                        indexer when indexing documents from the web for
 *                        example. Usually the most important terms will occur
 *                        early on in a document so you can often safely
 *                        ignore the terms in a field after a certain number
 *                        of them. If you wanted to speed up indexing and same
 *                        space in your index you may only want to index the
 *                        first 1000 terms in a field. On the other hand, if
 *                        you want to be more thorough and you are indexing
 *                        documents from your file-system you may set this
 *                        parameter to Ferret::FIX_INT_MAX.
 *  use_compound_file::   Default: true. Uses a compound file to store the
 *                        index. This prevents an error being raised for
 *                        having too many files open at the same time. The
 *                        default is true but performance is better if this is
 *                        set to false.
 *
 *
 *  === Deleting Documents
 *
 *  Both IndexReader and FrtIndexWriter allow you to delete documents. You should
 *  use the IndexReader to delete documents by document id and FrtIndexWriter to
 *  delete documents by term which we'll explain now. It is preferrable to
 *  delete documents from an index using FrtIndexWriter for performance reasons.
 *  To delete documents using the FrtIndexWriter you should give each document in
 *  the index a unique ID. If you are indexing documents from the file-system
 *  this unique ID will be the full file path. If indexing documents from the
 *  database you should use the primary key as the ID field. You can then
 *  use the delete method to delete a file referenced by the ID. For example;
 *
 *    index_writer.delete(:id, "/path/to/indexed/file")
 */
void Init_IndexWriter(void) {
    id_boost = rb_intern("boost");

    sym_create            = ID2SYM(rb_intern("create"));
    sym_create_if_missing = ID2SYM(rb_intern("create_if_missing"));
    sym_field_infos       = ID2SYM(rb_intern("field_infos"));

    sym_chunk_size        = ID2SYM(rb_intern("chunk_size"));
    sym_max_buffer_memory = ID2SYM(rb_intern("max_buffer_memory"));
    sym_index_interval    = ID2SYM(rb_intern("term_index_interval"));
    sym_skip_interval     = ID2SYM(rb_intern("doc_skip_interval"));
    sym_merge_factor      = ID2SYM(rb_intern("merge_factor"));
    sym_max_buffered_docs = ID2SYM(rb_intern("max_buffered_docs"));
    sym_max_merge_docs    = ID2SYM(rb_intern("max_merge_docs"));
    sym_max_field_length  = ID2SYM(rb_intern("max_field_length"));
    sym_use_compound_file = ID2SYM(rb_intern("use_compound_file"));

    cIndexWriter = rb_define_class_under(mIndex, "IndexWriter", rb_cObject);
    rb_define_alloc_func(cIndexWriter, frb_iw_alloc);

    rb_define_const(cIndexWriter, "WRITE_LOCK_TIMEOUT", INT2FIX(1));
    rb_define_const(cIndexWriter, "COMMIT_LOCK_TIMEOUT", INT2FIX(10));
    rb_define_const(cIndexWriter, "WRITE_LOCK_NAME", rb_str_new2(FRT_WRITE_LOCK_NAME));
    rb_define_const(cIndexWriter, "COMMIT_LOCK_NAME", rb_str_new2(FRT_COMMIT_LOCK_NAME));
    rb_define_const(cIndexWriter, "DEFAULT_CHUNK_SIZE", INT2FIX(frt_default_config.chunk_size));
    rb_define_const(cIndexWriter, "DEFAULT_MAX_BUFFER_MEMORY", INT2FIX(frt_default_config.max_buffer_memory));
    rb_define_const(cIndexWriter, "DEFAULT_TERM_INDEX_INTERVAL", INT2FIX(frt_default_config.index_interval));
    rb_define_const(cIndexWriter, "DEFAULT_DOC_SKIP_INTERVAL", INT2FIX(frt_default_config.skip_interval));
    rb_define_const(cIndexWriter, "DEFAULT_MERGE_FACTOR", INT2FIX(frt_default_config.merge_factor));
    rb_define_const(cIndexWriter, "DEFAULT_MAX_BUFFERED_DOCS", INT2FIX(frt_default_config.max_buffered_docs));
    rb_define_const(cIndexWriter, "DEFAULT_MAX_MERGE_DOCS", INT2FIX(frt_default_config.max_merge_docs));
    rb_define_const(cIndexWriter, "DEFAULT_MAX_FIELD_LENGTH", INT2FIX(frt_default_config.max_field_length));
    rb_define_const(cIndexWriter, "DEFAULT_USE_COMPOUND_FILE", frt_default_config.use_compound_file ? Qtrue : Qfalse);

    rb_define_method(cIndexWriter, "initialize",   frb_iw_init, -1);
    rb_define_method(cIndexWriter, "doc_count",    frb_iw_get_doc_count, 0);
    rb_define_method(cIndexWriter, "close",        frb_iw_close, 0);
    rb_define_method(cIndexWriter, "add_document", frb_iw_add_doc, 1);
    rb_define_method(cIndexWriter, "<<",           frb_iw_add_doc, 1);
    rb_define_method(cIndexWriter, "optimize",     frb_iw_optimize, 0);
    rb_define_method(cIndexWriter, "commit",       frb_iw_commit, 0);
    rb_define_method(cIndexWriter, "add_readers",  frb_iw_add_readers, 1);
    rb_define_method(cIndexWriter, "delete",       frb_iw_delete, 2);
    rb_define_method(cIndexWriter, "field_infos",  frb_iw_field_infos, 0);
    rb_define_method(cIndexWriter, "analyzer",     frb_iw_get_analyzer, 0);
    rb_define_method(cIndexWriter, "analyzer=",    frb_iw_set_analyzer, 1);
    rb_define_method(cIndexWriter, "version",      frb_iw_version, 0);

    rb_define_method(cIndexWriter, "chunk_size",  frb_iw_get_chunk_size, 0);
    rb_define_method(cIndexWriter, "chunk_size=", frb_iw_set_chunk_size, 1);

    rb_define_method(cIndexWriter, "max_buffer_memory",  frb_iw_get_max_buffer_memory, 0);
    rb_define_method(cIndexWriter, "max_buffer_memory=", frb_iw_set_max_buffer_memory, 1);

    rb_define_method(cIndexWriter, "term_index_interval",  frb_iw_get_index_interval, 0);
    rb_define_method(cIndexWriter, "term_index_interval=", frb_iw_set_index_interval, 1);

    rb_define_method(cIndexWriter, "doc_skip_interval",  frb_iw_get_skip_interval, 0);
    rb_define_method(cIndexWriter, "doc_skip_interval=", frb_iw_set_skip_interval, 1);

    rb_define_method(cIndexWriter, "merge_factor",  frb_iw_get_merge_factor, 0);
    rb_define_method(cIndexWriter, "merge_factor=", frb_iw_set_merge_factor, 1);

    rb_define_method(cIndexWriter, "max_buffered_docs",  frb_iw_get_max_buffered_docs, 0);
    rb_define_method(cIndexWriter, "max_buffered_docs=", frb_iw_set_max_buffered_docs, 1);

    rb_define_method(cIndexWriter, "max_merge_docs",  frb_iw_get_max_merge_docs, 0);
    rb_define_method(cIndexWriter, "max_merge_docs=", frb_iw_set_max_merge_docs, 1);

    rb_define_method(cIndexWriter, "max_field_length",  frb_iw_get_max_field_length, 0);
    rb_define_method(cIndexWriter, "max_field_length=", frb_iw_set_max_field_length, 1);

    rb_define_method(cIndexWriter, "use_compound_file",  frb_iw_get_use_compound_file, 0);
    rb_define_method(cIndexWriter, "use_compound_file=", frb_iw_set_use_compound_file, 1);
}

/*
 *  Document-class: Ferret::Index::IndexReader
 *
 *  == Summary
 *
 *  IndexReader is used for reading data from the index. This class is usually
 *  used directly for more advanced tasks like iterating through terms in an
 *  index, accessing term-vectors or deleting documents by document id. It is
 *  also used internally by IndexSearcher.
 */
void Init_IndexReader(void) {
    cIndexReader = rb_define_class_under(mIndex, "IndexReader", rb_cObject);
    rb_define_alloc_func(cIndexReader, frb_ir_alloc);
    rb_define_method(cIndexReader, "initialize",     frb_ir_init,          1);
    rb_define_method(cIndexReader, "set_norm",       frb_ir_set_norm,      3);
    rb_define_method(cIndexReader, "norms",          frb_ir_norms,         1);
    rb_define_method(cIndexReader, "get_norms_into", frb_ir_get_norms_into, 3);
    rb_define_method(cIndexReader, "commit",         frb_ir_commit,        0);
    rb_define_method(cIndexReader, "close",          frb_ir_close,         0);
    rb_define_method(cIndexReader, "has_deletions?", frb_ir_has_deletions, 0);
    rb_define_method(cIndexReader, "delete",         frb_ir_delete,        1);
    rb_define_method(cIndexReader, "deleted?",       frb_ir_is_deleted,    1);
    rb_define_method(cIndexReader, "max_doc",        frb_ir_max_doc,       0);
    rb_define_method(cIndexReader, "num_docs",       frb_ir_num_docs,      0);
    rb_define_method(cIndexReader, "undelete_all",   frb_ir_undelete_all,  0);
    rb_define_method(cIndexReader, "latest?",        frb_ir_is_latest,     0);
    rb_define_method(cIndexReader, "get_document",   frb_ir_get_doc,      -1);
    rb_define_method(cIndexReader, "[]",             frb_ir_get_doc,      -1);
    rb_define_method(cIndexReader, "term_vector",    frb_ir_term_vector,   2);
    rb_define_method(cIndexReader, "term_vectors",   frb_ir_term_vectors,  1);
    rb_define_method(cIndexReader, "term_docs",      frb_ir_term_docs,     0);
    rb_define_method(cIndexReader, "term_positions", frb_ir_term_positions, 0);
    rb_define_method(cIndexReader, "term_docs_for",  frb_ir_term_docs_for, 2);
    rb_define_method(cIndexReader, "term_positions_for", frb_ir_t_pos_for, 2);
    rb_define_method(cIndexReader, "doc_freq",       frb_ir_doc_freq,      2);
    rb_define_method(cIndexReader, "terms",          frb_ir_terms,         1);
    rb_define_method(cIndexReader, "terms_from",     frb_ir_terms_from,    2);
    rb_define_method(cIndexReader, "term_count",     frb_ir_term_count,    1);
    rb_define_method(cIndexReader, "fields",         frb_ir_fields,        0);
    rb_define_method(cIndexReader, "field_names",    frb_ir_fields,        0);
    rb_define_method(cIndexReader, "field_infos",    frb_ir_field_infos,   0);
    rb_define_method(cIndexReader, "tokenized_fields", frb_ir_tk_fields,   0);
    rb_define_method(cIndexReader, "version",        frb_ir_version,       0);
    rb_define_method(cIndexReader, "each",           frb_ir_each,          0);
    rb_define_method(cIndexReader, "to_enum",        frb_ir_to_enum,       0);
}

/* rdoc hack
extern VALUE mFerret = rb_define_module("Ferret");
*/

/*
 *  Document-module: Ferret::Index
 *
 *  == Summary
 *
 *  The Index module contains all the classes used for adding to and
 *  retrieving from the index. The important classes to know about are;
 *
 *  * FieldInfo
 *  * FieldInfos
 *  * IndexWriter
 *  * IndexReader
 *  * LazyDoc
 *
 *  The other classes in this module are useful for more advanced uses like
 *  building tag clouds, creating more-like-this queries, custom highlighting
 *  etc. They are also useful for index browsers.
 */
void Init_Index(void) {
    mIndex = rb_define_module_under(mFerret, "Index");

    sym_boost     = ID2SYM(rb_intern("boost"));
    sym_analyzer  = ID2SYM(rb_intern("analyzer"));
    sym_close_dir = ID2SYM(rb_intern("close_dir"));
    fsym_content  = rb_intern("content");

    Init_TermVector();
    Init_TermEnum();
    Init_TermDocEnum();

    Init_FieldInfos();

    Init_LazyDoc();
    Init_IndexWriter();
    Init_IndexReader();
}
