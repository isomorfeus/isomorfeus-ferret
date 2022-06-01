#include "frt_index.h"
#include "isomorfeus_ferret.h"

VALUE cFieldInfo;

static VALUE sym_store;
static VALUE sym_index;
static VALUE sym_compression;
static VALUE sym_brotli;
static VALUE sym_bz2;
static VALUE sym_lz4;
static VALUE sym_term_vector;
static VALUE sym_omit_norms;
static VALUE sym_untokenized;
static VALUE sym_untokenized_omit_norms;
static VALUE sym_with_offsets;
static VALUE sym_with_positions;
static VALUE sym_with_positions_offsets;

extern VALUE sym_boost;

static void frb_fi_free(void *p) {
    frt_fi_deref((FrtFieldInfo *)p);
}

static void frb_fi_get_params(VALUE roptions, FrtStoreValue *store, FrtCompressionType *compression, FrtIndexValue *index, FrtTermVectorValue *term_vector, float *boost) {
    VALUE v;
    Check_Type(roptions, T_HASH);
    v = rb_hash_aref(roptions, sym_boost);
    if (Qnil != v) {
        *boost = (float)NUM2DBL(v);
    } else {
        *boost = 1.0f;
    }
    v = rb_hash_aref(roptions, sym_store);
    if (Qnil != v) Check_Type(v, T_SYMBOL);
    if (v == sym_no || v == sym_false || v == Qfalse) {
        *store = FRT_STORE_NO;
    } else if (v == sym_yes || v == sym_true || v == Qtrue) {
        *store = FRT_STORE_YES;
    } else if (v == Qnil) {
        /* leave as default */
    } else {
        rb_raise(rb_eArgError, ":%s isn't a valid argument for :store. Please choose from [:yes, :no]",
                 rb_id2name(SYM2ID(v)));
    }

    v = rb_hash_aref(roptions, sym_compression);
    if (Qnil != v) Check_Type(v, T_SYMBOL);
    if (v == sym_no || v == sym_false || v == Qfalse) {
        *compression = FRT_COMPRESSION_NONE;
    } else if (v == sym_yes || v == sym_true || v == Qtrue || v == sym_brotli) {
        *compression = FRT_COMPRESSION_BROTLI;
    } else if (v == sym_bz2) {
        *compression = FRT_COMPRESSION_BZ2;
    } else if (v == sym_lz4) {
        *compression = FRT_COMPRESSION_LZ4;
    } else if (v == Qnil) {
        /* leave as default */
    } else {
        rb_raise(rb_eArgError, ":%s isn't a valid argument for :compression. Please choose from [:yes, :no, :brotli, :bz2, :lz4]",
                 rb_id2name(SYM2ID(v)));
    }

    v = rb_hash_aref(roptions, sym_index);
    if (Qnil != v) Check_Type(v, T_SYMBOL);
    if (v == sym_no || v == sym_false || v == Qfalse) {
        *index = FRT_INDEX_NO;
    } else if (v == sym_yes || v == sym_true || v == Qtrue) {
        *index = FRT_INDEX_YES;
    } else if (v == sym_untokenized) {
        *index = FRT_INDEX_UNTOKENIZED;
    } else if (v == sym_omit_norms) {
        *index = FRT_INDEX_YES_OMIT_NORMS;
    } else if (v == sym_untokenized_omit_norms) {
        *index = FRT_INDEX_UNTOKENIZED_OMIT_NORMS;
    } else if (v == Qnil) {
        /* leave as default */
    } else {
        rb_raise(rb_eArgError, ":%s isn't a valid argument for :index. Please choose from [:no, :yes, :untokenized, "
                 ":omit_norms, :untokenized_omit_norms]", rb_id2name(SYM2ID(v)));
    }

    v = rb_hash_aref(roptions, sym_term_vector);
    if (Qnil != v) Check_Type(v, T_SYMBOL);
    if (v == sym_no || v == sym_false || v == Qfalse) {
        *term_vector = FRT_TERM_VECTOR_NO;
    } else if (v == sym_yes || v == sym_true || v == Qtrue) {
        *term_vector = FRT_TERM_VECTOR_YES;
    } else if (v == sym_with_positions) {
        *term_vector = FRT_TERM_VECTOR_WITH_POSITIONS;
    } else if (v == sym_with_offsets) {
        *term_vector = FRT_TERM_VECTOR_WITH_OFFSETS;
    } else if (v == sym_with_positions_offsets) {
        *term_vector = FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS;
    } else if (v == Qnil) {
        /* leave as default */
        if (*index == FRT_INDEX_NO) *term_vector = FRT_TERM_VECTOR_NO;
    } else {
        rb_raise(rb_eArgError, ":%s isn't a valid argument for :term_vector. Please choose from [:no, :yes, "
                 ":with_positions, :with_offsets, :with_positions_offsets]", rb_id2name(SYM2ID(v)));
    }
}

static size_t frb_fi_size(const void *p) {
    return sizeof(FrtFieldInfo);
    (void)p;
}

const rb_data_type_t frb_field_info_t = {
    .wrap_struct_name = "FrbFieldInfo",
    .function = {
        .dmark = NULL,
        .dfree = frb_fi_free,
        .dsize = frb_fi_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_get_field_info(FrtFieldInfo *fi) {
    if (fi) {
        if (fi->rfi == 0 || fi->rfi == Qnil) {
            fi->rfi = TypedData_Wrap_Struct(cFieldInfo, &frb_field_info_t, fi);
            FRT_REF(fi);
        }
        return fi->rfi;
    }
    return Qnil;
}

/*
 *  call-seq:
 *     FieldInfo.new(name, options = {}) -> field_info
 *
 *  Create a new FieldInfo object with the name +name+ and the properties
 *  specified in +options+. The available options are [:store, :compression,
 *  :index, :term_vector, :boost]. See the description of FieldInfo for more
 *  information on these properties.
 */
static VALUE frb_fi_alloc(VALUE rclass) {
    FrtFieldInfo *fi = frt_fi_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_field_info_t, fi);
}

static VALUE frb_fi_init(int argc, VALUE *argv, VALUE self) {
    VALUE roptions, rname;
    FrtFieldInfo *fi;
    TypedData_Get_Struct(self, FrtFieldInfo, &frb_field_info_t, fi);
    FrtStoreValue store = FRT_STORE_YES;
    FrtCompressionType compression = FRT_COMPRESSION_NONE;
    FrtIndexValue index = FRT_INDEX_YES;
    FrtTermVectorValue term_vector = FRT_TERM_VECTOR_WITH_POSITIONS_OFFSETS;
    float boost = 1.0f;

    rb_scan_args(argc, argv, "11", &rname, &roptions);
    if (argc > 1) {
        frb_fi_get_params(roptions, &store, &compression, &index, &term_vector, &boost);
    }
    fi = frt_fi_init(fi, frb_field(rname), store, compression, index, term_vector);
    fi->boost = boost;
    fi->rfi = self;
    return self;
}

/*
 *  call-seq:
 *     fi.name -> symbol
 *
 *  Return the name of the field
 */
static VALUE frb_fi_name(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return ID2SYM(fi->name);
}

/*
 *  call-seq:
 *     fi.stored? -> bool
 *
 *  Return true if the field is stored in the index.
 */
static VALUE frb_fi_is_stored(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_is_stored(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.compressed? -> bool
 *
 *  Return true if the field is stored in the index in compressed format.
 */
static VALUE frb_fi_is_compressed(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_is_compressed(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.indexed? -> bool
 *
 *  Return true if the field is indexed, ie searchable in the index.
 */
static VALUE frb_fi_is_indexed(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_is_indexed(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.tokenized? -> bool
 *
 *  Return true if the field is tokenized. Tokenizing is the process of
 *  breaking the field up into tokens. That is "the quick brown fox" becomes:
 *
 *    ["the", "quick", "brown", "fox"]
 *
 *  A field can only be tokenized if it is indexed.
 */
static VALUE frb_fi_is_tokenized(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_is_tokenized(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.omit_norms? -> bool
 *
 *  Return true if the field omits the norm file. The norm file is the file
 *  used to store the field boosts for an indexed field. If you do not boost
 *  any fields, and you can live without scoring based on field length then
 *  you can omit the norms file. This will give the index a slight performance
 *  boost and it will use less memory, especially for indexes which have a
 *  large number of documents.
 */
static VALUE frb_fi_omit_norms(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_omit_norms(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.store_term_vector? -> bool
 *
 *  Return true if the term-vectors are stored for this field.
 */
static VALUE frb_fi_store_term_vector(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_store_term_vector(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.store_positions? -> bool
 *
 *  Return true if positions are stored with the term-vectors for this field.
 */
static VALUE frb_fi_store_positions(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_store_positions(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.store_offsets? -> bool
 *
 *  Return true if offsets are stored with the term-vectors for this field.
 */
static VALUE frb_fi_store_offsets(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_store_offsets(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.has_norms? -> bool
 *
 *  Return true if this field has a norms file. This is the same as calling;
 *
 *    fi.indexed? and not fi.omit_norms?
 */
static VALUE frb_fi_has_norms(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return fi_has_norms(fi) ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *     fi.boost -> boost
 *
 *  Return the default boost for this field
 */
static VALUE frb_fi_boost(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    return rb_float_new((double)fi->boost);
}

/*
 *  call-seq:
 *     fi.to_s -> string
 *
 *  Return a string representation of the FieldInfo object.
 */
static VALUE frb_fi_to_s(VALUE self) {
    FrtFieldInfo *fi = (FrtFieldInfo *)DATA_PTR(self);
    char *fi_s = frt_fi_to_s(fi);
    VALUE rfi_s = rb_str_new2(fi_s);
    free(fi_s);
    return rfi_s;
}

/*
 *  Document-class: Ferret::Index::FieldInfo
 *
 *  == Summary
 *
 *  The FieldInfo class is the field descriptor for the index. It specifies
 *  whether a field is compressed or not or whether it should be indexed and
 *  tokenized. Every field has a name which must be a symbol. There are three
 *  properties that you can set, +:store+, +:index+ and +:term_vector+. You
 *  can also set the default +:boost+ for a field as well.
 *
 *  == Properties
 *
 *  === :store
 *
 *  The +:store+ property allows you to specify how a field is stored. You can
 *  leave a field unstored (+:no+), store it in it's original format (+:yes+)
 *  or store it in compressed format (+:compressed+). By default the document
 *  is stored in its original format. If the field is large and it is stored
 *  elsewhere where it is easily accessible you might want to leave it
 *  unstored. This will keep the index size a lot smaller and make the
 *  indexing process a lot faster. For example, you should probably leave the
 *  +:content+ field unstored when indexing all the documents in your
 *  file-system.
 *
 *  === :index
 *
 *  The +:index+ property allows you to specify how a field is indexed. A
 *  field must be indexed to be searchable. However, a field doesn't need to
 *  be indexed to be store in the Ferret index. You may want to use the index
 *  as a simple database and store things like images or MP3s in the index. By
 *  default each field is indexed and tokenized (split into tokens) (+:yes+).
 *  If you don't want to index the field use +:no+. If you want the field
 *  indexed but not tokenized, use +:untokenized+. Do this for the fields you
 *  wish to sort by. There are two other values for +:index+; +:omit_norms+
 *  and +:untokenized_omit_norms+. These values correspond to +:yes+ and
 *  +:untokenized+ respectively and are useful if you are not boosting any
 *  fields and you'd like to speed up the index. The norms file is the file
 *  which contains the boost values for each document for a particular field.
 *
 *  === :term_vector
 *
 *  See TermVector for a description of term-vectors. You can specify whether
 *  or not you would like to store term-vectors. The available options are
 *  +:no+, +:yes+, +:with_positions+, +:with_offsets+ and
 *  +:with_positions_offsets+. Note that you need to store the positions to
 *  associate offsets with individual terms in the term_vector.
 *
 *  == Property Table
 *
 *    Property       Value                     Description
 *    ------------------------------------------------------------------------
 *     :store       | :no                     | Don't store field
 *                  |                         |
 *                  | :yes (default)          | Store field in its original
 *                  |                         | format. Use this value if you
 *                  |                         | want to highlight matches.
 *                  |                         | or print match excerpts a la
 *                  |                         | Google search.
 *     -------------|-------------------------|------------------------------
 *     :compression | :no (default)           | Don't compress stored field
 *                  |                         |
 *                  | :brotli                 | Compress field using Brotli
 *                  |                         |
 *                  | :bz2                    | Compress field using BZip2
 *                  |                         |
 *                  | :lz4                    | Compress field using LZ4
 *     -------------|-------------------------|------------------------------
 *     :index       | :no                     | Do not make this field
 *                  |                         | searchable.
 *                  |                         |
 *                  | :yes (default)          | Make this field searchable and
 *                  |                         | tokenized its contents.
 *                  |                         |
 *                  | :untokenized            | Make this field searchable but
 *                  |                         | do not tokenize its contents.
 *                  |                         | use this value for fields you
 *                  |                         | wish to sort by.
 *                  |                         |
 *                  | :omit_norms             | Same as :yes except omit the
 *                  |                         | norms file. The norms file can
 *                  |                         | be omitted if you don't boost
 *                  |                         | any fields and you don't need
 *                  |                         | scoring based on field length.
 *                  |                         |
 *                  | :untokenized_omit_norms | Same as :untokenized except omit
 *                  |                         | the norms file. Norms files can
 *                  |                         | be omitted if you don't boost
 *                  |                         | any fields and you don't need
 *                  |                         | scoring based on field length.
 *                  |                         |
 *     -------------|-------------------------|------------------------------
 *     :term_vector | :no                     | Don't store term-vectors
 *                  |                         |
 *                  | :yes                    | Store term-vectors without
 *                  |                         | storing positions or offsets.
 *                  |                         |
 *                  | :with_positions         | Store term-vectors with
 *                  |                         | positions.
 *                  |                         |
 *                  | :with_offsets           | Store term-vectors with
 *                  |                         | offsets.
 *                  |                         |
 *                  | :with_positions_offsets | Store term-vectors with
 *                  | (default)               | positions and offsets.
 *     -------------|-------------------------|------------------------------
 *     :boost       | Float                   | The boost property is used to
 *                  |                         | set the default boost for a
 *                  |                         | field. This boost value will
 *                  |                         | used for all instances of the
 *                  |                         | field in the index unless
 *                  |                         | otherwise specified when you
 *                  |                         | create the field. All values
 *                  |                         | should be positive.
 *                  |                         |
 *
 *  == Examples
 *
 *    fi = FieldInfo.new(:title, :index => :untokenized, :term_vector => :no,
 *                       :boost => 10.0)
 *
 *    fi = FieldInfo.new(:content)
 *
 *    fi = FieldInfo.new(:created_on, :index => :untokenized_omit_norms,
 *                       :term_vector => :no)
 *
 *    fi = FieldInfo.new(:image, :store => :yes, :compression => :brotli, :index => :no,
 *                       :term_vector => :no)
 */
static void Init_FieldInfo(void) {
    sym_store = ID2SYM(rb_intern("store"));
    sym_index = ID2SYM(rb_intern("index"));
    sym_term_vector = ID2SYM(rb_intern("term_vector"));

    sym_brotli = ID2SYM(rb_intern("brotli"));
    sym_bz2 = ID2SYM(rb_intern("bz2"));
    sym_lz4 = ID2SYM(rb_intern("lz4"));
    // sym_level = ID2SYM(rb_intern("level"));
    sym_compression = ID2SYM(rb_intern("compression"));

    sym_untokenized = ID2SYM(rb_intern("untokenized"));
    sym_omit_norms = ID2SYM(rb_intern("omit_norms"));
    sym_untokenized_omit_norms = ID2SYM(rb_intern("untokenized_omit_norms"));

    sym_with_positions = ID2SYM(rb_intern("with_positions"));
    sym_with_offsets = ID2SYM(rb_intern("with_offsets"));
    sym_with_positions_offsets = ID2SYM(rb_intern("with_positions_offsets"));

    cFieldInfo = rb_define_class_under(mIndex, "FieldInfo", rb_cObject);
    rb_define_alloc_func(cFieldInfo, frb_fi_alloc);

    rb_define_method(cFieldInfo, "initialize",  frb_fi_init, -1);
    rb_define_method(cFieldInfo, "name",        frb_fi_name, 0);
    rb_define_method(cFieldInfo, "stored?",     frb_fi_is_stored, 0);
    rb_define_method(cFieldInfo, "compressed?", frb_fi_is_compressed, 0);
    rb_define_method(cFieldInfo, "indexed?",    frb_fi_is_indexed, 0);
    rb_define_method(cFieldInfo, "tokenized?",  frb_fi_is_tokenized, 0);
    rb_define_method(cFieldInfo, "omit_norms?", frb_fi_omit_norms, 0);
    rb_define_method(cFieldInfo, "store_term_vector?",
                                                frb_fi_store_term_vector, 0);
    rb_define_method(cFieldInfo, "store_positions?",
                                                frb_fi_store_positions, 0);
    rb_define_method(cFieldInfo, "store_offsets?",
                                                frb_fi_store_offsets, 0);
    rb_define_method(cFieldInfo, "has_norms?",  frb_fi_has_norms, 0);
    rb_define_method(cFieldInfo, "boost",       frb_fi_boost, 0);
    rb_define_method(cFieldInfo, "to_s",        frb_fi_to_s, 0);
}
