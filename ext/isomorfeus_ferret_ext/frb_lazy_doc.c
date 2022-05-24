#include "frt_index.h"
#include "isomorfeus_ferret.h"

static ID id_fields;
FrtLazyDoc empty_lazy_doc = {0};
VALUE cLazyDoc;

/****************************************************************************
 *
 * LazyDoc Methods
 *
 ****************************************************************************/

static void frb_ld_free(void *p) {
  if (p && p != &empty_lazy_doc) frt_lazy_doc_close((FrtLazyDoc *)p);
}

VALUE frb_get_lazy_doc(FrtLazyDoc *lazy_doc) {
  VALUE rld = rb_class_new_instance(0, NULL, cLazyDoc);
  ((struct RData *)(rld))->data = lazy_doc;
  return rld;
}

static VALUE frb_ld_init(VALUE self) {
  rb_call_super(0, NULL);
  ((struct RData *)(self))->data = &empty_lazy_doc;
  ((struct RData *)(self))->dfree = frb_ld_free;
  return self;
}

static VALUE frb_ld_df_load(VALUE self, VALUE rkey, FrtLazyDocField *lazy_df) {
  VALUE rdata = Qnil;

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
  rb_hash_aset(self, rkey, rdata);

  return rdata;
}

/*
 *  call-seq:
 *     lazy_doc.load -> lazy_doc
 *
 *  Load all unloaded fields in the document from the index.
 */
static VALUE frb_ld_load(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (ld->loaded) return self;
  int i;
  FrtLazyDocField *lazy_df;
  for (i = 0; i < ld->size; i++) {
    lazy_df = ld->fields[i];
    if (!lazy_df->loaded) frb_ld_df_load(self, ID2SYM(lazy_df->name), lazy_df);
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
  FrtLazyDoc *ld = DATA_PTR(self);
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

static VALUE frb_ld_lt(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_lte(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_eql(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_gt(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_gte(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_get(VALUE self, VALUE key) {
  VALUE rval = rb_hash_lookup(self, key);
  if (rval == Qnil) {
    FrtLazyDoc *ld = DATA_PTR(self);
    ID dfkey = SYM2ID(key);
    FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
    if (df) rval = frb_ld_df_load(self, dfkey, df);
  }
  return rval;
}

static VALUE frb_ld_any(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (argc == 0) {
    return (ld->size > 0) ? Qtrue : Qfalse;
  } else {
      if (!ld->loaded) frb_ld_load(self);
      return rb_call_super(argc, argv);
  }
}

static VALUE frb_ld_assoc(VALUE self, VALUE key) {
  FrtLazyDoc *ld = DATA_PTR(self);
  ID dfkey = SYM2ID(key);
  VALUE key_obj = key;
  FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
  if (df && !df->loaded) frb_ld_df_load(self, dfkey, df);
  return rb_call_super(1, &key_obj);
}

static VALUE frb_ld_compact(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_dig(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  VALUE key = argv[0];
  ID dfkey = SYM2ID(key);
  FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
  if (df && !df->loaded) frb_ld_df_load(self, dfkey, df);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_each(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_empty(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  return (ld->size == 0) ? Qtrue : Qfalse;
}

static VALUE frb_ld_eqlq(VALUE self, VALUE other) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE other_obj = other;
  return rb_call_super(1, &other_obj);
}

static VALUE frb_ld_except(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_fetch(int argc, VALUE *argv, VALUE self) {
  VALUE key = argv[0];
  if (argc == 1) return frb_ld_get(self, key);
  else {
    FrtLazyDoc *ld = DATA_PTR(self);
    ID dfkey = SYM2ID(key);
    FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
    if (df && !df->loaded) frb_ld_df_load(self, dfkey, df);
    return rb_call_super(argc, argv);
  }
}

static VALUE frb_ld_fetch_values(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_filter(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_flatten(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_has_key(VALUE self, VALUE key) {
  VALUE hk = Qfalse;
  FrtLazyDoc *ld = DATA_PTR(self);
  ID dfkey = SYM2ID(key);
  FrtLazyDocField *df = frt_h_get(ld->field_dictionary, (void *)dfkey);
  if (df) hk = Qtrue;
  return hk;
}

static VALUE frb_ld_has_value(VALUE self, VALUE value) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE value_obj = value;
  return rb_call_super(1, &value_obj);
}

static VALUE frb_ld_hash(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_inspect(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_invert(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_key(VALUE self, VALUE value) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE value_obj = value;
  return rb_call_super(1, &value_obj);
}

static VALUE frb_ld_length(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  return INT2FIX(ld->size);
}

static VALUE frb_ld_merge(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_rassoc(VALUE self, VALUE value) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  VALUE value_obj = value;
  return rb_call_super(1, &value_obj);
}

static VALUE frb_ld_reject(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_select(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_slice(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_to_a(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_to_h(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_to_hash(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_to_proc(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_transform_keys(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_transform_values(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
}

static VALUE frb_ld_values(VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(0, NULL);
}

static VALUE frb_ld_values_at(int argc, VALUE *argv, VALUE self) {
  FrtLazyDoc *ld = DATA_PTR(self);
  if (!ld->loaded) frb_ld_load(self);
  return rb_call_super(argc, argv);
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
  id_fields = rb_intern("@fields");

  cLazyDoc = rb_define_class_under(mIndex, "LazyDoc", rb_cHash);
  rb_define_method(cLazyDoc, "initialize", frb_ld_init,   0);
  rb_define_method(cLazyDoc, "load",     frb_ld_load,     0);
  rb_define_method(cLazyDoc, "fields",   frb_ld_fields,   0);
  rb_define_method(cLazyDoc, "keys",     frb_ld_fields,   0);
  rb_define_method(cLazyDoc, "<",        frb_ld_lt,       1);
  rb_define_method(cLazyDoc, "<=",       frb_ld_lte,      1);
  rb_define_method(cLazyDoc, "==",       frb_ld_eql,      1);
  rb_define_method(cLazyDoc, ">",        frb_ld_gt,       1);
  rb_define_method(cLazyDoc, ">=",       frb_ld_gte,      1);
  rb_define_method(cLazyDoc, "[]",       frb_ld_get,      1);
  rb_undef_method(cLazyDoc,  "[]=");
  rb_define_method(cLazyDoc, "any?",     frb_ld_any,     -1);
  rb_define_method(cLazyDoc, "assoc",    frb_ld_assoc,    1);
  rb_undef_method(cLazyDoc,  "clear");
  rb_define_method(cLazyDoc, "compact",  frb_ld_compact,  0);
  rb_undef_method(cLazyDoc,  "compact!");
  // #compare_by_identity
  // #compare_by_identity?
  // #deconstruct_keys
  // #default
  // #default=
  // #default_proc
  // #default_proc=
  rb_undef_method(cLazyDoc,  "delete");
  rb_undef_method(cLazyDoc,  "delete_if");
  rb_define_method(cLazyDoc, "dig",      frb_ld_dig,     -1);
  rb_define_method(cLazyDoc, "each",     frb_ld_each,     0);
  rb_define_method(cLazyDoc, "each_key", frb_ld_each,     0);
  rb_define_method(cLazyDoc, "each_pair", frb_ld_each,    0);
  rb_define_method(cLazyDoc, "each_value", frb_ld_each,   0);
  rb_define_method(cLazyDoc, "empty?",   frb_ld_empty,    0);
  rb_define_method(cLazyDoc, "eql?",     frb_ld_eqlq,     0);
  rb_define_method(cLazyDoc, "except",   frb_ld_except,  -1);
  rb_define_method(cLazyDoc, "fetch",    frb_ld_fetch,   -1);
  rb_define_method(cLazyDoc, "fetch_values", frb_ld_fetch_values, -1);
  rb_define_method(cLazyDoc, "filter",   frb_ld_filter,   0);
  rb_undef_method(cLazyDoc,  "filter!");
  rb_define_method(cLazyDoc, "flatten",  frb_ld_flatten,  0);
  rb_define_method(cLazyDoc, "has_key?", frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "has_value?", frb_ld_has_value, 1);
  rb_define_method(cLazyDoc, "hash",     frb_ld_hash,     1);
  rb_define_method(cLazyDoc, "include?", frb_ld_has_key,  1);
  rb_undef_method(cLazyDoc,  "initialize_copy");
  rb_define_method(cLazyDoc, "inspect",  frb_ld_inspect,  0);
  rb_define_method(cLazyDoc, "invert",   frb_ld_invert,   0);
  rb_undef_method(cLazyDoc,  "keep_if");
  rb_define_method(cLazyDoc, "key",      frb_ld_key,      1);
  rb_define_method(cLazyDoc, "key?",     frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "length",   frb_ld_length,   0);
  rb_define_method(cLazyDoc, "member?",  frb_ld_has_key,  1);
  rb_define_method(cLazyDoc, "merge",    frb_ld_merge,   -1);
  rb_undef_method(cLazyDoc,  "merge!");
  rb_define_method(cLazyDoc, "rassoc",   frb_ld_rassoc,   1);
  rb_undef_method(cLazyDoc,  "rehash");
  rb_define_method(cLazyDoc, "reject",   frb_ld_reject,   0);
  rb_undef_method(cLazyDoc,  "reject!");
  rb_undef_method(cLazyDoc,  "replace");
  rb_define_method(cLazyDoc, "select",   frb_ld_select,   0);
  rb_undef_method(cLazyDoc,  "select!");
  rb_undef_method(cLazyDoc,  "shift");
  rb_define_method(cLazyDoc, "size",     frb_ld_length,   0);
  rb_define_method(cLazyDoc, "slice",    frb_ld_slice,   -1);
  rb_undef_method(cLazyDoc,  "store");
  rb_define_method(cLazyDoc, "to_a",     frb_ld_to_a,     0);
  rb_define_method(cLazyDoc, "to_h",     frb_ld_to_h,     0);
  rb_define_method(cLazyDoc, "to_hash",  frb_ld_to_hash,  0);
  rb_define_method(cLazyDoc, "to_proc",  frb_ld_to_proc,  0);
  rb_define_method(cLazyDoc, "to_s",     frb_ld_inspect,  0);
  rb_define_method(cLazyDoc, "transform_keys", frb_ld_transform_keys, -1);
  rb_undef_method(cLazyDoc,  "transform_keys!");
  rb_define_method(cLazyDoc, "transform_values", frb_ld_transform_values, -1);
  rb_undef_method(cLazyDoc,  "transform_values!");
  rb_undef_method(cLazyDoc,  "update");
  rb_define_method(cLazyDoc, "value?",   frb_ld_has_value, 1);
  rb_define_method(cLazyDoc, "values",   frb_ld_values,   0);
  rb_define_method(cLazyDoc, "values_at", frb_ld_values_at, 0);
}
