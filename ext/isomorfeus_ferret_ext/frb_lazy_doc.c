#include "frt_index.h"
#include "isomorfeus_ferret.h"

extern VALUE rb_hash_update(int argc, VALUE *argv, VALUE self);

extern VALUE sym_each;
extern ID id_eql;

static ID id_compact;
static ID id_equal;
static ID id_except;
static ID id_fields;
static ID id_flatten;
static ID id_ge;
static ID id_get;
static ID id_gt;
static ID id_inspect;
static ID id_invert;
static ID id_le;
static ID id_merge_bang;
static ID id_reject;
static ID id_select;
static ID id_size;
static ID id_slice;
static ID id_to_proc;
static ID id_transform_keys;
static ID id_transform_values;

FrtLazyDoc empty_lazy_doc = {0};
VALUE cLazyDoc;

typedef struct rLazyDoc {
  FrtHash    *hash;
  FrtLazyDoc *doc;
} rLazyDoc;

/****************************************************************************
 *
 * LazyDoc Methods
 *
 ****************************************************************************/

static void frb_ld_free(void *p) {
  rLazyDoc *rld = (rLazyDoc *)p;
  if (rld->doc != &empty_lazy_doc) {
    frt_lazy_doc_close(rld->doc);
  }
  frt_h_destroy(rld->hash);
  free(rld);
}

static size_t frb_ld_size(const void *p) {
  return sizeof(rLazyDoc);
  (void)p;
}

void rld_mark(void *key, void *value, void *arg) {
  rb_gc_mark((VALUE)value);
}

static void frb_ld_mark(void *p) {
  frt_h_each(((rLazyDoc *)p)->hash, rld_mark, NULL);
}

const rb_data_type_t frb_ld_t = {
  .wrap_struct_name = "FrbLazyDoc",
  .function = {
    .dmark = frb_ld_mark,
    .dfree = frb_ld_free,
    .dsize = frb_ld_size,
    .dcompact = NULL,
    .reserved = {0},
  },
  .parent = NULL,
  .data = NULL,
  .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

VALUE frb_get_lazy_doc(FrtLazyDoc *lazy_doc) {
  rLazyDoc *rld = FRT_ALLOC(rLazyDoc);
  rld->hash = frt_h_new_ptr(NULL);
  rld->doc = lazy_doc;
  return TypedData_Wrap_Struct(cLazyDoc, &frb_ld_t, rld);
}

static VALUE frb_ld_alloc(VALUE rclass) {
  rLazyDoc *rld = FRT_ALLOC(rLazyDoc);
  rld->hash = frt_h_new_ptr(NULL);
  rld->doc = &empty_lazy_doc;
  return TypedData_Wrap_Struct(rclass, &frb_ld_t, rld);
}

static VALUE frb_ld_df_load(VALUE self, VALUE rkey, FrtLazyDocField *lazy_df) {
  rLazyDoc *rld = DATA_PTR(self);
  VALUE rdata;
  if (lazy_df->size == 1) {
    char *data = frt_lazy_df_get_data(lazy_df, 0);
    rdata = rb_str_new(data, lazy_df->data[0].length);
    rb_enc_associate(rdata, lazy_df->data[0].encoding);
  } else {
    int i;
    VALUE rstr;
    rdata = rb_ary_new2(lazy_df->size);
    for (i = 0; i < lazy_df->size; i++) {
      char *data = frt_lazy_df_get_data(lazy_df, i);
      rstr = rb_str_new(data, lazy_df->data[i].length);
      rb_enc_associate(rstr, lazy_df->data[i].encoding);
      rb_ary_store(rdata, i, rstr);
    }
  }
  frt_h_set(rld->hash, (void *)rkey, (void *)rdata);
  return rdata;
}

/*
 *  call-seq:
 *     lazy_doc.load -> lazy_doc
 *
 *  Load all unloaded fields in the document from the index.
 */
static VALUE frb_ld_load(VALUE self) {
  rLazyDoc *rld = DATA_PTR(self);
  FrtLazyDoc *ld = rld->doc;
  if (ld->loaded) return self;
  int i;
  FrtLazyDocField *lazy_df;
  for (i = 0; i < ld->size; i++) {
    lazy_df = ld->fields[i];
    if (!(lazy_df->loaded)) frb_ld_df_load(self, ID2SYM(lazy_df->name), lazy_df);
  }
  ld->loaded = true;
  return self;
}

/*
 *  call-seq:
 *     lazy_doc.fields -> array of available fields
 *
 *  Returns the list of fields stored for this particular document. If you try
 *  to access any of these fields in the document the field will be loaded.
 *  Try to access any other field an nil will be returned.
 */
static VALUE frb_ld_fields(VALUE self) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  VALUE rfields = rb_ivar_get(self, id_fields);
  if (rfields == Qnil) {
    int i;
    rfields = rb_ary_new2(ld->size);
    for (i = 0; i < ld->size; i++) {
      rb_ary_store(rfields, i, ID2SYM(ld->fields[i]->name));
    }
    rb_ivar_set(self, id_fields, rfields);
  }
  return rfields;
}

void rld_to_hash(void *key, void *value, void *arg) {
  rb_hash_aset((VALUE)arg, (VALUE)key, (VALUE)value);
}

static VALUE frb_ld_to_h(VALUE self) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE hash = rb_hash_new();
  frt_h_each(rld->hash, rld_to_hash, (void *)hash);
  return hash;
}

static VALUE frb_ld_lt(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
  if (ld->size < other_rld->doc->size) return Qtrue;
  VALUE self_h = frb_ld_to_h(self);
  VALUE other_h = frb_ld_to_h(other);
  return rb_funcall(self_h, id_lt, 1, other_h);
}

static VALUE frb_ld_le(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
  if (ld->size <= other_rld->doc->size) return Qtrue;
  VALUE self_h = frb_ld_to_h(self);
  VALUE other_h = frb_ld_to_h(other);
  return rb_funcall(self_h, id_le, 1, other_h);
}

static VALUE frb_ld_equal(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  int other_size;
  VALUE other_h;
  if (TYPE(other) == T_HASH) {
    other_h = other;
    other_size = FIX2INT(rb_funcall(other_h, id_size, 0));
  } else {
    TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
    other_h = frb_ld_to_h(other);
    other_size = other_rld->doc->size;
  }
  if (ld->size == other_size) {
    VALUE self_h = frb_ld_to_h(self);
    return rb_funcall(self_h, id_equal, 1, other_h);
  }
  return Qfalse;
}

static VALUE frb_ld_gt(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
  if (ld->size > other_rld->doc->size) return Qtrue;
  VALUE self_h = frb_ld_to_h(self);
  VALUE other_h = frb_ld_to_h(other);
  return rb_funcall(self_h, id_gt, 1, other_h);
}

static VALUE frb_ld_ge(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
  if (ld->size >= other_rld->doc->size) return Qtrue;
  VALUE self_h = frb_ld_to_h(self);
  VALUE other_h = frb_ld_to_h(other);
  return rb_funcall(self_h, id_ge, 1, other_h);
}

static VALUE frb_ld_get(VALUE self, VALUE key) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  VALUE rval = (VALUE)frt_h_get(rld->hash, (void *)key);
  if (rval) return rval;
  if (TYPE(key) != T_SYMBOL) rb_raise(rb_eArgError, "key, must be a symbol");
  FrtLazyDocField *df = frt_h_get(rld->doc->field_dictionary, (void *)SYM2ID(key));
  if (df) return frb_ld_df_load(self, key, df);
  return Qnil;
}

void rld_any(void *key, void *value, void *arg) {
  VALUE *v = arg;
  *v = rb_yield_values(2, (VALUE)key, (VALUE)value);
}

static VALUE frb_ld_assoc(VALUE self, VALUE key) {
  rLazyDoc *rld = DATA_PTR(self);
  VALUE value = (VALUE)frt_h_get(rld->hash, (void *)key);
  if (!value) {
    FrtLazyDoc *ld = rld->doc;
    FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)SYM2ID(key));
    if (!df) return Qnil;
    if (df && !df->loaded) value = frb_ld_df_load(self, key, df);
  }
  VALUE a[2] = {key, value};
  return rb_ary_new_from_values(2, a);
}

static VALUE frb_ld_any(int argc, VALUE *argv, VALUE self) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  FrtLazyDoc *ld = rld->doc;
  if (argc == 0) {
    if (!rb_block_given_p()) {
      return (ld->size > 0) ? Qtrue : Qfalse;
    } else {
      if (!ld->loaded) frb_ld_load(self);
      VALUE res = Qnil;
      frt_h_each(rld->hash, rld_any, &res);
      if (RTEST(res)) return Qtrue;
      else return Qfalse;
    }
  } else if (argc == 1) {
    VALUE obj = argv[0];
    VALUE key = rb_funcall(obj, id_get, 1, 0);
    VALUE a = frb_ld_assoc(self, key);
    return rb_funcall(a, id_equal, 1, obj);
  }
  rb_raise(rb_eArgError, "at most one arg may be given");
  return Qfalse;
}

static VALUE frb_ld_compact(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_compact, 0);
}

static VALUE frb_ld_dig(int argc, VALUE *argv, VALUE self) {
  if (argc == 0) rb_raise(rb_eArgError, "at least a key must be given");
  VALUE key = argv[0];
  if (TYPE(key) != T_SYMBOL) rb_raise(rb_eArgError, "first arg, key, must be a symbol");
  VALUE value = frb_ld_get(self, key);
  if (argc == 1) return value;
  if (TYPE(value) == T_ARRAY && argc == 2) {
    return rb_ary_entry(value, NUM2LONG(argv[1]));
  }
  return Qnil;
}

static VALUE frb_ld_to_enum(VALUE self) {
  return rb_enumeratorize(self, sym_each, 0, NULL);
}

void rld_each(void *key, void *value, void *arg) {
  rb_yield_values(2, (VALUE)key, (VALUE)value);
}

static VALUE frb_ld_each(VALUE self) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  FrtLazyDoc *ld = rld->doc;
  if (!ld->loaded) frb_ld_load(self);
  if (rb_block_given_p()) {
    frt_h_each(rld->hash, rld_each, NULL);
    return self;
  } else {
    return frb_ld_to_enum(self);
  }
}

static VALUE frb_ld_empty(VALUE self) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  return (ld->size == 0) ? Qtrue : Qfalse;
}

static VALUE frb_ld_eql(VALUE self, VALUE other) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  rLazyDoc *other_rld;
  int other_size;
  VALUE other_h;
  if (TYPE(other) == T_HASH) {
    other_h = other;
    other_size = FIX2INT(rb_funcall(other_h, id_size, 0));
  } else {
    TypedData_Get_Struct(other, rLazyDoc, &frb_ld_t, other_rld);
    other_h = frb_ld_to_h(other);
    other_size = other_rld->doc->size;
  }
  if (ld->size == other_size) {
    VALUE self_h = frb_ld_to_h(self);
    return rb_funcall(self_h, id_eql, 1, other_h);
  }
  return Qfalse;
}

static VALUE frb_ld_except(int argc, VALUE *argv, VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcallv(hash, id_except, argc, argv);
}

static VALUE frb_ld_fetch(int argc, VALUE *argv, VALUE self) {
  VALUE key = argv[0];
  if (TYPE(key) != T_SYMBOL) rb_raise(rb_eArgError, "first arg must be a symbol");
  VALUE res = frb_ld_get(self, key);
  if (argc == 1) {
    if (res == Qnil && rb_block_given_p()) return rb_yield(key);
    return res;
  }
  if (argc == 2) {
    if (res == Qnil) return argv[1];
    return res;
  }
  rb_raise(rb_eArgError, "too many args, only two allowed: key, default_value");
}

static VALUE frb_ld_fetch_values(int argc, VALUE *argv, VALUE self) {
  rLazyDoc *rld = DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE ary = rb_ary_new();
  int i;
  VALUE value;
  for (i=0; i<argc; i++) {
    value = (VALUE)frt_h_get(rld->hash, (void *)argv[i]);
    if (value) rb_ary_push(ary, value);
    else if (rb_block_given_p()) {
      value = rb_yield(argv[i]);
      rb_ary_push(ary, value);
    }
  }
  if (FIX2INT(rb_funcall(ary, id_size, 0)) == 0) rb_raise(rb_eException, "nothing found for given keys");
  return ary;
}

static VALUE frb_ld_filter(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_select, 0);
}

void rld_flatten(void *key, void *value, void *arg) {
  rb_ary_push((VALUE)arg, (VALUE)key);
  rb_ary_push((VALUE)arg, (VALUE)value);
}

static VALUE frb_ld_flatten(int argc, VALUE *argv, VALUE self) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE ary = rb_ary_new();
  frt_h_each(rld->hash, rld_flatten, (void *)ary);
  if (argc == 1) {
    int level = FIX2INT(argv[0]) - 1;
    VALUE rlevel = INT2FIX(level);
    rb_funcall(ary, id_flatten, 1, rlevel);
  }
  return ary;
}

static VALUE frb_ld_has_key(VALUE self, VALUE key) {
  if (TYPE(key) != T_SYMBOL) rb_raise(rb_eArgError, "arg must be a symbol");
  VALUE hk = Qfalse;
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  ID dfkey = SYM2ID(key);
  FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
  if (df) hk = Qtrue;
  return hk;
}

static VALUE frb_ld_has_value(VALUE self, VALUE value) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  FrtLazyDoc *ld = rld->doc;
  if (!ld->loaded) frb_ld_load(self);
  int i;
  VALUE hvalue;
  for (i=0; i<ld->size; i++) {
    hvalue = (VALUE)frt_h_get(rld->hash, (void *)ID2SYM(ld->fields[i]->name));
    hvalue = rb_funcall(hvalue, id_equal, 1, value);
    if (hvalue == Qtrue) return Qtrue;
  }
  return Qfalse;
}

static VALUE frb_ld_inspect(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_inspect, 0);
}

static VALUE frb_ld_invert(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_invert, 0);
}

static VALUE frb_ld_key(VALUE self, VALUE value) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  FrtLazyDoc *ld = rld->doc;
  if (!ld->loaded) frb_ld_load(self);
  int i;
  VALUE hvalue;
  for (i=0; i<ld->size; i++) {
    hvalue = (VALUE)frt_h_get(rld->hash, (void *)ID2SYM(ld->fields[i]->name));
    hvalue = rb_funcall(hvalue, id_equal, 1, value);
    if (hvalue == Qtrue) return ID2SYM(ld->fields[i]->name);
  }
  return Qnil;
}

static VALUE frb_ld_length(VALUE self) {
  FrtLazyDoc *ld = ((rLazyDoc *)DATA_PTR(self))->doc;
  return INT2FIX(ld->size);
}

static VALUE frb_ld_merge(int argc, VALUE *argv, VALUE self) {
  rLazyDoc *rld = (rLazyDoc *)DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE hash = frb_ld_to_h(self);
  return rb_funcallv(hash, id_merge_bang, argc, argv);
}

static VALUE frb_ld_rassoc(VALUE self, VALUE value) {
  VALUE key = frb_ld_key(self, value);
  if (key == Qnil) return Qnil;
  VALUE a[2] = {key, value};
  return rb_ary_new_from_values(2, a);
}

void rld_reject(void *key, void *value, void *arg) {
  VALUE res = rb_yield_values(2, (VALUE)key, (VALUE)value);
  if (!RTEST(res)) rb_hash_aset((VALUE)arg, (VALUE)key, (VALUE)value);
}

static VALUE frb_ld_reject(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_reject, 0);
}

static VALUE frb_ld_slice(int argc, VALUE *argv, VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcallv(hash, id_slice, argc, argv);
}

void rld_to_a(void *key, void *value, void *arg) {
  VALUE ary = rb_ary_new();
  rb_ary_push(ary, (VALUE)key);
  rb_ary_push(ary, (VALUE)value);
  rb_ary_push((VALUE)arg, ary);
}

static VALUE frb_ld_to_a(VALUE self) {
  rLazyDoc *rld = DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE ary = rb_ary_new();
  frt_h_each(rld->hash, rld_to_a, (void *)ary);
  return ary;
}

static VALUE frb_ld_to_proc(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_to_proc, 0);
}

static VALUE frb_ld_transform_keys(int argc, VALUE *argv, VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcallv(hash, id_transform_keys, argc, argv);
}

static VALUE frb_ld_transform_values(VALUE self) {
  VALUE hash = frb_ld_to_h(self);
  return rb_funcall(hash, id_transform_values, 0);
}

void rld_values(void *key, void *value, void *arg) {
  rb_ary_push((VALUE)arg, (VALUE)value);
}

static VALUE frb_ld_values(VALUE self) {
  rLazyDoc *rld = DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE ary = rb_ary_new();
  frt_h_each(rld->hash, rld_values, (void *)ary);
  return ary;
}

static VALUE frb_ld_values_at(int argc, VALUE *argv, VALUE self) {
  rLazyDoc *rld = DATA_PTR(self);
  if (!rld->doc->loaded) frb_ld_load(self);
  VALUE ary = rb_ary_new();
  int i;
  VALUE value;
  for (i=0; i<argc; i++) {
    value = (VALUE)frt_h_get(rld->hash, (void *)argv[i]);
    if (value) rb_ary_push(ary, value);
  	else rb_ary_push(ary, Qnil);
  }
  return ary;
}

/*
 *  Document-class: Ferret::Index::LazyDoc
 *
 *  == Summary
 *
 *  When a document is retrieved from the index a LazyDoc is returned.
 *  It inherits from rubys Hash class, however it is read only.
 *  LazyDoc lazily adds fields to itself when they are accessed or
 *  automatically loads all fields if needed.
 *  To load all fields use the LazyDoc#load method.
 *  Methods from the Hash class, that would modify the LazyDoc itself,
 *  are not supported, .
 *
 *  == Example
 *
 *    doc = index_reader[0]
 *
 *    doc.keys     #=> []
 *    doc.values   #=> []
 *    doc.fields   #=> [:title, :content]
 *
 *    title = doc[:title] #=> "the title"
 *    doc.keys     #=> [:title]
 *    doc.values   #=> ["the title"]
 *    doc.fields   #=> [:title, :content]
 *
 *    doc.load
 *    doc.keys     #=> [:title, :content]
 *    doc.values   #=> ["the title", "the content"]
 *    doc.fields   #=> [:title, :content]
 */
void Init_LazyDoc(void) {
  id_compact = rb_intern("compact");
  id_equal = rb_intern("==");
  id_except = rb_intern("except");
  id_fields = rb_intern("@fields");
  id_flatten = rb_intern("flatten");
  id_ge = rb_intern(">=");
  id_get = rb_intern("[]");
  id_gt = rb_intern(">");
  id_inspect = rb_intern("inspect");
  id_invert = rb_intern("invert");
  id_le = rb_intern("<=");
  id_merge_bang = rb_intern("merge!");
  id_reject = rb_intern("reject");
  id_select = rb_intern("select");
  id_size = rb_intern("size");
  id_slice = rb_intern("slice");
  id_to_proc = rb_intern("to_proc");
  id_transform_keys = rb_intern("transform_keys");
  id_transform_values = rb_intern("transform_values");

  cLazyDoc = rb_define_class_under(mIndex, "LazyDoc", rb_cObject);
  rb_include_module(cLazyDoc, rb_mEnumerable);
  rb_define_alloc_func(cLazyDoc, frb_ld_alloc);
  rb_define_method(cLazyDoc, "load",     frb_ld_load,     0);
  rb_define_method(cLazyDoc, "fields",   frb_ld_fields,   0);
  rb_define_method(cLazyDoc, "keys",     frb_ld_fields,   0);
  rb_define_method(cLazyDoc, "<",        frb_ld_lt,       1);
  rb_define_method(cLazyDoc, "<=",       frb_ld_le,       1);
  rb_define_method(cLazyDoc, "==",       frb_ld_equal,    1);
  rb_define_method(cLazyDoc, ">",        frb_ld_gt,       1);
  rb_define_method(cLazyDoc, ">=",       frb_ld_ge,       1);
  rb_define_method(cLazyDoc, "[]",       frb_ld_get,      1);
  rb_define_method(cLazyDoc, "any?",     frb_ld_any,     -1);
  rb_define_method(cLazyDoc, "assoc",    frb_ld_assoc,    1);
  rb_define_method(cLazyDoc, "compact",  frb_ld_compact,  0);
  rb_define_method(cLazyDoc, "dig",      frb_ld_dig,     -1);
  rb_define_method(cLazyDoc, "each",     frb_ld_each,     0);
  rb_define_method(cLazyDoc, "each_key", frb_ld_each,     0);
  rb_define_method(cLazyDoc, "each_pair", frb_ld_each,    0);
  rb_define_method(cLazyDoc, "each_value", frb_ld_each,   0);
  rb_define_method(cLazyDoc, "empty?",   frb_ld_empty,    0);
  rb_define_method(cLazyDoc, "eql?",     frb_ld_eql,      1);
  rb_define_method(cLazyDoc, "except",   frb_ld_except,  -1);
  rb_define_method(cLazyDoc, "fetch",    frb_ld_fetch,   -1);
  rb_define_method(cLazyDoc, "fetch_values", frb_ld_fetch_values, -1);
  rb_define_method(cLazyDoc, "filter",   frb_ld_filter,   0);
  rb_define_method(cLazyDoc, "flatten",  frb_ld_flatten, -1);
  rb_define_method(cLazyDoc, "has_key?", frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "has_value?", frb_ld_has_value, 1);
  rb_define_method(cLazyDoc, "include?", frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "inspect",  frb_ld_inspect,  0);
  rb_define_method(cLazyDoc, "invert",   frb_ld_invert,   0);
  rb_define_method(cLazyDoc, "key",      frb_ld_key,      1);
  rb_define_method(cLazyDoc, "key?",     frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "length",   frb_ld_length,   0);
  rb_define_method(cLazyDoc, "member?",  frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "merge",    frb_ld_merge,   -1);
  rb_define_method(cLazyDoc, "rassoc",   frb_ld_rassoc,   1);
  rb_define_method(cLazyDoc, "reject",   frb_ld_reject,   0);
  rb_define_method(cLazyDoc, "select",   frb_ld_filter,   0);
  rb_define_method(cLazyDoc, "size",     frb_ld_length,   0);
  rb_define_method(cLazyDoc, "slice",    frb_ld_slice,   -1);
  rb_define_method(cLazyDoc, "to_a",     frb_ld_to_a,     0);
  rb_define_method(cLazyDoc, "to_enum",  frb_ld_to_enum,  0);
  rb_define_method(cLazyDoc, "to_h",     frb_ld_to_h,     0);
  rb_define_method(cLazyDoc, "to_hash",  frb_ld_to_h,     0);
  rb_define_method(cLazyDoc, "to_proc",  frb_ld_to_proc,  0);
  rb_define_method(cLazyDoc, "to_s",     frb_ld_inspect,  0);
  rb_define_method(cLazyDoc, "transform_keys", frb_ld_transform_keys, -1);
  rb_define_method(cLazyDoc, "transform_values", frb_ld_transform_values, 0);
  rb_define_method(cLazyDoc, "value?",   frb_ld_has_value, 1);
  rb_define_method(cLazyDoc, "values",   frb_ld_values,   0);
  rb_define_method(cLazyDoc, "values_at", frb_ld_values_at, -1);
}
