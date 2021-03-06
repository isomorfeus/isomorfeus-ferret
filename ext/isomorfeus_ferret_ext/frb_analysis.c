#include "frt_analysis.h"
#include "isomorfeus_ferret.h"
#include <ruby.h>
#include <ruby/re.h>

static VALUE mAnalysis;

static VALUE cToken;
static VALUE cLetterTokenizer;
static VALUE cWhiteSpaceTokenizer;
static VALUE cStandardTokenizer;
static VALUE cRegExpTokenizer;

static VALUE cLowerCaseFilter;
static VALUE cStopFilter;
static VALUE cMappingFilter;
static VALUE cHyphenFilter;
static VALUE cStemFilter;

static VALUE cAnalyzer;
static VALUE cLetterAnalyzer;
static VALUE cWhiteSpaceAnalyzer;
static VALUE cStandardAnalyzer;
static VALUE cPerFieldAnalyzer;
static VALUE cRegExpAnalyzer;

static VALUE cTokenStream;

/* TokenStream Methods */
static ID id_next;
static ID id_reset;
static ID id_clone;
static ID id_text;

/* FrtAnalyzer Methods */
static ID id_token_stream;

static VALUE object_space;

extern rb_encoding *utf8_encoding;
extern int ruby_re_search(struct re_pattern_buffer *, const char *, int, int, int, struct re_registers *);

int frb_rb_hash_size(VALUE hash) {
#ifdef RHASH_SIZE
    return RHASH_SIZE(hash);
#else
    return RHASH(hash)->ntbl->num_entries;
#endif
}

/****************************************************************************
 *
 * Utility Methods
 *
 ****************************************************************************/

static char **get_stopwords(VALUE rstop_words) {
    char **stop_words;
    int i, len;
    VALUE rstr;
    Check_Type(rstop_words, T_ARRAY);
    len = RARRAY_LEN(rstop_words);
    stop_words = FRT_ALLOC_N(char *, RARRAY_LEN(rstop_words) + 1);
    stop_words[len] = NULL;
    for (i = 0; i < len; i++) {
        rstr = rb_obj_as_string(RARRAY_PTR(rstop_words)[i]);
        stop_words[i] = rs2s(rstr);
    }
    return stop_words;
}

/****************************************************************************
 *
 * token methods
 *
 ****************************************************************************/

static void frb_token_free(void *p) {
    free(p);
}

static size_t frb_token_size(const void *p) {
    return sizeof(FrtToken);
    (void)p;
}

const rb_data_type_t frb_token_t = {
    .wrap_struct_name = "FrbToken",
    .function = {
        .dmark = NULL,
        .dfree = frb_token_free,
        .dsize = frb_token_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_token_alloc(VALUE rclass) {
    FrtToken *tk = frt_tk_new();
    return TypedData_Wrap_Struct(rclass, &frb_token_t, tk);
}

FrtToken *frb_set_token(FrtToken *tk, VALUE rt) {
    FrtToken *rtk;
    if (rt == Qnil)
        return NULL;
    TypedData_Get_Struct(rt, FrtToken, &frb_token_t, rtk);
    frt_tk_set(tk, rtk->text, rtk->len, rtk->start, rtk->end, rtk->pos_inc, utf8_encoding);
    return tk;
}

/*
 *  call-seq:
 *     Token.new(text, start, end, pos_inc = 1) -> new Token
 *
 *  Creates a new token setting the text, start and end offsets of the token
 *  and the position increment for the token.
 *
 *  The position increment is usually set to 1 but you can set it to other
 *  values as needed.  For example, if you have a stop word filter you will be
 *  skipping tokens. Let's say you have the stop words "the" and "and" and you
 *  parse the title "The Old Man and the Sea". The terms "Old", "Man" and
 *  "Sea" will have the position increments 2, 1 and 3 respectively.
 *
 *  Another reason you might want to vary the position increment is if you are
 *  adding synonyms to the index. For example let's say you have the synonym
 *  group "quick", "fast" and "speedy". When tokenizing the phrase "Next day
 *  speedy delivery", you'll add "speedy" first with a position increment of 1
 *  and then "fast" and "quick" with position increments of 0 since they are
 *  represented in the same position.
 *
 *  The offset set values +start+ and +end+ should be byte offsets, not
 *  character offsets. This makes it easy to use those offsets to quickly
 *  access the token in the input string and also to insert highlighting tags
 *  when necessary.
 *
 *  text::       the main text for the token.
 *  start::      the start offset of the token in bytes.
 *  end::        the end offset of the token in bytes.
 *  pos_inc::    the position increment of a token. See above.
 *  return::     a newly created and assigned Token object
 */
static VALUE frb_token_init(int argc, VALUE *argv, VALUE self) {
    FrtToken *tk;
    char *text;
    int pos_inc = 1;
    VALUE rtext, rstart, rend, rpos_inc;
    switch (rb_scan_args(argc, argv, "31", &rtext, &rstart, &rend, &rpos_inc)) {
        case 4: pos_inc = FIX2INT(rpos_inc);
    }
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    text = rs2s(rtext);
    frt_tk_set(tk, text, strlen(text), FIX2INT(rstart), FIX2INT(rend), pos_inc, rb_enc_get(rtext));
    return self;
}

/*
 *  call-seq:
 *     token.cmp(other_token) -> bool
 *
 *  Used to compare two tokens. Token is extended by Comparable so you can
 *  also use +<+, +>+, +<=+, +>=+ etc. to compare tokens.
 *
 *  Tokens are sorted by the position in the text at which they occur, ie
 *  the start offset. If two tokens have the same start offset, (see
 *  pos_inc=) then, they are sorted by the end offset and then
 *  lexically by the token text.
 */
static VALUE frb_token_cmp(VALUE self, VALUE rother) {
    FrtToken *tk, *other;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    TypedData_Get_Struct(rother, FrtToken, &frb_token_t, other);
    return INT2FIX(frt_tk_cmp(tk, other));
}

/*
 *  call-seq:
 *     token.text -> text
 *
 *  Returns the text that this token represents
 */
static VALUE frb_token_get_text(VALUE self) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    VALUE rtext = rb_str_new2(tk->text);
    rb_enc_associate(rtext, utf8_encoding);
    return rtext;
}

/*
 *  call-seq:
 *     token.text = text -> text
 *
 *  Set the text for this token.
 */
static VALUE frb_token_set_text(VALUE self, VALUE rtext) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    char *text = rs2s(rtext);
    frt_tk_set(tk, text, strlen(text), tk->start, tk->end, tk->pos_inc, rb_enc_get(rtext));
    return frb_token_get_text(self);
}

/*
 *  call-seq:
 *     token.start -> integer
 *
 *  Start byte-position of this token
 */
static VALUE frb_token_get_start_offset(VALUE self) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    return INT2FIX(tk->start);
}

/*
 *  call-seq:
 *     token.end -> integer
 *
 *  End byte-position of this token
 */
static VALUE frb_token_get_end_offset(VALUE self) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    return INT2FIX(tk->end);
}

/*
 *  call-seq:
 *     token.pos_inc -> integer
 *
 *  Position Increment for this token
 */
static VALUE frb_token_get_pos_inc(VALUE self) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    return INT2FIX(tk->pos_inc);
}

/*
 *  call-seq:
 *     token.start = start -> integer
 *
 *  Set start byte-position of this token
 */
static VALUE frb_token_set_start_offset(VALUE self, VALUE rstart) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    tk->start = FIX2INT(rstart);
    return rstart;
}

/*
 *  call-seq:
 *     token.end = end -> integer
 *
 *  Set end byte-position of this token
 */
static VALUE frb_token_set_end_offset(VALUE self, VALUE rend) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    tk->end = FIX2INT(rend);
    return rend;
}

/*
 *  call-seq:
 *     token.pos_inc = pos_inc -> integer
 *
 *  Set the position increment.  This determines the position of this token
 *  relative to the previous Token in a TokenStream, used in phrase
 *  searching.
 *
 *  The default value is 1.
 *
 *  Some common uses for this are:
 *
 *  * Set it to zero to put multiple terms in the same position.  This is
 *    useful if, e.g., a word has multiple stems.  Searches for phrases
 *    including either stem will match.  In this case, all but the first
 *    stem's increment should be set to zero: the increment of the first
 *    instance should be one.  Repeating a token with an increment of zero
 *    can also be used to boost the scores of matches on that token.
 *
 *  * Set it to values greater than one to inhibit exact phrase matches.
 *    If, for example, one does not want phrases to match across removed
 *    stop words, then one could build a stop word filter that removes stop
 *    words and also sets the increment to the number of stop words removed
 *    before each non-stop word.  Then exact phrase queries will only match
 *    when the terms occur with no intervening stop words.
 *
 */
static VALUE frb_token_set_pos_inc(VALUE self, VALUE rpos_inc) {
    FrtToken *tk;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    tk->pos_inc = FIX2INT(rpos_inc);
    return rpos_inc;
}

/*
 *  call-seq:
 *     token.to_s -> token_str
 *
 *  Return a string representation of the token
 */
static VALUE frb_token_to_s(VALUE self) {
    FrtToken *tk;
    char *buf;
    VALUE rstr;
    TypedData_Get_Struct(self, FrtToken, &frb_token_t, tk);
    buf = alloca(strlen(tk->text) + 80);
    sprintf(buf, "token[\"%s\":%d:%d:%d]", tk->text, (int)tk->start, (int)tk->end, tk->pos_inc);
    rstr = rb_str_new2(buf);
    rb_enc_associate(rstr, utf8_encoding);
    return rstr;
}

/****************************************************************************
 *
 * TokenStream Methods
 *
 ****************************************************************************/

static void frb_ts_free(void *p) {
    frt_ts_deref((FrtTokenStream *)p);
}

static size_t frb_ts_size(const void *p) {
    return sizeof(FrtTokenStream);
    (void)p;
}

typedef struct RegExpTokenStream {
    FrtTokenStream super;
    VALUE regex;
    VALUE proc;
    long  curr_ind;
} RegExpTokenStream;

static void frb_rets_mark(void *p) {
    RegExpTokenStream *ts = (RegExpTokenStream *)p;
    rb_gc_mark(ts->regex);
    rb_gc_mark(ts->proc);
}

static size_t frb_rets_size(const void *p) {
    return sizeof(RegExpTokenStream);
    (void)p;
}

const rb_data_type_t frb_token_stream_t = {
    .wrap_struct_name = "FrbTokenStream",
    .function = {
        .dmark = NULL,
        .dfree = frb_ts_free,
        .dsize = frb_ts_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

const rb_data_type_t frb_reg_exp_token_stream_t = {
    .wrap_struct_name = "FrbRegExpTokenStream",
    .function = {
        .dmark = frb_rets_mark,
        .dfree = frb_ts_free,
        .dsize = frb_rets_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = &frb_token_stream_t,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static FrtToken *rets_next(FrtTokenStream *ts);

static VALUE get_rb_token_stream(FrtTokenStream *ts) {
    VALUE rts = ts->rts;

    if (rts == 0 || rts == Qnil) {
        if (ts->next == &rets_next) {
            rts = TypedData_Wrap_Struct(cTokenStream, &frb_reg_exp_token_stream_t, ts);
        } else {
            rts = TypedData_Wrap_Struct(cTokenStream, &frb_token_stream_t, ts);
        }
        ts->rts = rts;
    }
    return rts;
}

/*
 *  call-seq:
 *     token_stream.text = text -> text
 *
 *  Set the text attribute of the TokenStream to the text you wish to be
 *  tokenized. For example, you may do this;
 *
 *      token_stream.text = File.read(file_name)
 */
static VALUE frb_ts_set_text(VALUE self, VALUE rtext) {
    FrtTokenStream *ts;
    // TypedData_Get_Struct(self, FrtTokenStream, &frb_token_stream_t, ts);
    ts = DATA_PTR(self);
    StringValue(rtext);
    ts->reset(ts, rs2s(rtext), rb_enc_get(rtext));
    return rtext;
}

/*
 *  call-seq:
 *     token_stream.text = text -> text
 *
 *  Return the text that the TokenStream is tokenizing
 */
static VALUE frb_ts_get_text(VALUE self) {
    VALUE rtext = Qnil;
    FrtTokenStream *ts;
    // TypedData_Get_Struct(self, FrtTokenStream, &frb_token_stream_t, ts);
    ts = DATA_PTR(self);
    if (ts->text) {
        rtext = rb_str_new2(ts->text);
        rb_enc_associate(rtext, ts->encoding);
    }
    return rtext;
}

/*
 *  call-seq:
 *     token_stream.next -> token
 *
 *  Return the next token from the TokenStream or nil if there are no more
 *  tokens.
 */
static VALUE frb_ts_next(VALUE self) {
    FrtTokenStream *ts = DATA_PTR(self);
    FrtToken *next_tk;
    FrtToken *tk = ts->next(ts);
    if (tk == NULL) {
        return Qnil;
    }
    next_tk = frt_tk_new();
    frt_tk_set(next_tk, tk->text, tk->len, tk->start, tk->end, tk->pos_inc, utf8_encoding);
    return TypedData_Wrap_Struct(cToken, &frb_token_t, next_tk);
}

/****************************************************************************
 * TokenFilter
 ****************************************************************************/

#define TkFilt(filter) ((FrtTokenFilter *)(filter))

static void frb_tf_mark(void *p) {
    FrtTokenStream *ts = (FrtTokenStream *)p;
    if (TkFilt(ts)->sub_ts && TkFilt(ts)->sub_ts->rts)
        rb_gc_mark(TkFilt(ts)->sub_ts->rts);
}

static void frb_tf_free(void *p) {
    frt_ts_deref((FrtTokenStream *)p);
}

static size_t frb_tf_size(const void *p) {
    return sizeof(FrtTokenFilter);
    (void)p;
}

const rb_data_type_t frb_token_filter_t = {
    .wrap_struct_name = "FrbTokenFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_tf_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

/****************************************************************************
 * Wrapped TokenStream
 ****************************************************************************/

static void cwrts_destroy_i(FrtTokenStream *ts) {
    rb_hash_delete(object_space, ((VALUE)ts)|1);
    free(ts);
}

static FrtToken *cwrts_next(FrtTokenStream *ts) {
    VALUE rtoken = rb_funcall(ts->rts, id_next, 0);
    return frb_set_token(&(ts->token), rtoken);
}

static FrtTokenStream *cwrts_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding) {
    ts->t = ts->text = text;
    ts->length = strlen(text);
    ts->encoding = encoding;
    rb_funcall(ts->rts, id_reset, 1, rb_str_new2(text));
    return ts;
}

static FrtTokenStream *cwrts_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *new_ts = frt_ts_clone_size(orig_ts, sizeof(FrtTokenStream));
    VALUE rts = new_ts->rts = rb_funcall(orig_ts->rts, id_clone, 0);
    rb_hash_aset(object_space, ((VALUE)new_ts)|1, rts);
    return new_ts;
}

static FrtTokenStream *frb_get_cwrapped_rts(VALUE rts) {
    FrtTokenStream *ts;
    if (frb_is_cclass(rts) && DATA_PTR(rts)) {
        // TypedData_Get_Struct(rts, FrtTokenStream, &frb_token_stream_t, ts);
        ts = DATA_PTR(rts);
        FRT_REF(ts);
    } else {
        ts = frt_ts_new_i(sizeof(FrtTokenStream));
        ts->rts = rts;
        ts->next = &cwrts_next;
        ts->reset = &cwrts_reset;
        ts->clone_i = &cwrts_clone_i;
        ts->destroy_i = &cwrts_destroy_i;
        /* prevent from being garbage collected */
        rb_hash_aset(object_space, ((VALUE)ts)|1, rts);
        ts->ref_cnt = 1;
    }
    return ts;
}

/****************************************************************************
 * RegExpTokenStream
 ****************************************************************************/

#define P "[_\\/.,-]"
#define HASDIGIT "\\w*\\d\\w*"
#define ALPHA "[-_[:alpha:]]"
#define ALNUM "[-_[:alnum:]]"

#define RETS(token_stream) ((RegExpTokenStream *)(token_stream))

static const char *TOKEN_RE =
    ALPHA "+(('" ALPHA "+)+|\\.(" ALPHA "\\.)+|"
    "(@|\\&)\\w+([-.]\\w+)*|:\\/\\/" ALNUM "+([-.\\/]" ALNUM "+)*)?"
    "|\\w+(([-._]\\w+)*\\@\\w+([-.]\\w+)+"
    "|" P HASDIGIT "(" P "\\w+" P HASDIGIT ")*(" P "\\w+)?"
    "|(\\.\\w+)+"
    "|"
    ")";
static VALUE rtoken_re;

static void rets_destroy_i(FrtTokenStream *ts) {
    rb_hash_delete(object_space, ((VALUE)ts)|1);
    free(ts);
}

/*
 *  call-seq:
 *     tokenizer.text = text -> text
 *
 *  Set the text to be tokenized by the tokenizer. The tokenizer gets reset to
 *  tokenize the text from the beginning.
 */
static VALUE frb_rets_set_text(VALUE self, VALUE rtext) {
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_reg_exp_token_stream_t, ts);
    StringValue(rtext);
    ts->reset(ts, rs2s(rtext), rb_enc_get(rtext));
    return rtext;
}

/*
 *  call-seq:
 *     tokenizer.text = text -> text
 *
 *  Get the text being tokenized by the tokenizer.
 */
static VALUE frb_rets_get_text(VALUE self) {
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_reg_exp_token_stream_t, ts);
    VALUE rstr = rb_str_new2(ts->text);
    rb_enc_associate(rstr, ts->encoding);
    return rstr;
}

// partly lifted from ruby 1.9 string.c
#include <ruby/encoding.h>
#define BEG(no) regs->beg[no]
#define END(no) regs->end[no]
#define STR_ENC_GET(str) rb_enc_from_index(ENCODING_GET(str))
static VALUE scan_once(VALUE rstr, VALUE pat, long *start) {
    VALUE match;
    struct re_registers *regs;
    if (rb_reg_search(pat, rstr, *start, 0) >= 0) {
        match = rb_backref_get();
        regs = RMATCH_REGS(match);
        if (BEG(0) == END(0)) {
            rb_encoding *enc = STR_ENC_GET(rstr);
            /* Always consume at least one character of the input string */
            if (RSTRING_LEN(rstr) > END(0))
                *start = END(0)+rb_enc_mbclen(RSTRING_PTR(rstr)+END(0), RSTRING_END(rstr), enc);
            else
                *start = END(0)+1;
        } else {
            *start = END(0);
        }
        return rb_reg_nth_match(0, match);
    }
  return Qnil;
}

static FrtToken *rets_next(FrtTokenStream *ts) {
    VALUE ret;
    long rtok_len;
    int beg, end;
    Check_Type(RETS(ts)->regex, T_REGEXP);

    VALUE rstr = rb_str_new_static(ts->text, ts->length);
    rb_enc_associate(rstr, ts->encoding);

    ret = scan_once(rstr, RETS(ts)->regex, &(RETS(ts)->curr_ind));

    if (NIL_P(ret))
        return NULL;

    Check_Type(ret, T_STRING);
    rtok_len = RSTRING_LEN(ret);
    beg = RETS(ts)->curr_ind - rtok_len;
    end = RETS(ts)->curr_ind;

    if (NIL_P(RETS(ts)->proc)) {
        return frt_tk_set(&(ts->token), rs2s(ret), rtok_len, beg, end, 1, rb_enc_get(ret));
    } else {
        VALUE rtok;
        rtok = rb_funcall(RETS(ts)->proc, id_call, 1, ret);
        return frt_tk_set(&(ts->token), rs2s(rtok), RSTRING_LEN(rtok), beg, end, 1, rb_enc_get(rtok));
    }
}

static FrtTokenStream *rets_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding) {
    frt_ts_reset(ts, text, encoding);
    RETS(ts)->curr_ind = 0;
    return ts;
}

static FrtTokenStream *rets_clone_i(FrtTokenStream *orig_ts) {
    FrtTokenStream *ts = frt_ts_clone_size(orig_ts, sizeof(RegExpTokenStream));
    return ts;
}

FrtTokenStream *rets_alloc(void) {
    return (FrtTokenStream *)frt_ecalloc(sizeof(RegExpTokenStream));
}

FrtTokenStream *rets_init(FrtTokenStream *ts, VALUE rtext, VALUE regex, VALUE proc) {
    ts = frt_ts_init(ts);
    ts->reset = &rets_reset;
    ts->next = &rets_next;
    ts->clone_i = &rets_clone_i;
    ts->destroy_i = &rets_destroy_i;

    if (rtext != Qnil) {
        rtext = StringValue(rtext);
        ts->reset(ts, rs2s(rtext), rb_enc_get(rtext));
    } else {
        RETS(ts)->curr_ind = 0;
    }

    RETS(ts)->proc = proc;

    if (NIL_P(regex)) {
        RETS(ts)->regex = rtoken_re;
    } else {
        Check_Type(regex, T_REGEXP);
        RETS(ts)->regex = regex;
    }
    return ts;
}

static FrtTokenStream *rets_new(VALUE rtext, VALUE regex, VALUE proc) {
    FrtTokenStream *ts = rets_alloc();
    return rets_init(ts, rtext, regex, proc);
}


static VALUE frb_reg_exp_tokenizer_alloc(VALUE rclass) {
    FrtTokenStream *ts = rets_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_reg_exp_token_stream_t, ts);
}

/*
 *  call-seq:
 *    RegExpTokenizer.new(input, /[[:alpha:]]+/)
 *
 *  Create a new tokenizer based on a regular expression
 *
 *  input::  text to tokenizer
 *  regexp:: regular expression used to recognize tokens in the input
 */
static VALUE frb_rets_init(int argc, VALUE *argv, VALUE self) {
    VALUE rtext, regex, proc;
    rb_scan_args(argc, argv, "11&", &rtext, &regex, &proc);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_reg_exp_token_stream_t, ts);
    rets_init(ts, rtext, regex, proc);
    ts->rts = self;
    return self;
}

/****************************************************************************
 * Tokenizers
 ****************************************************************************/

/*
 *  call-seq:
 *     LetterTokenizer.new() -> tokenizer
 *
 *  Create a new LetterTokenizer.
 */
static VALUE frb_letter_tokenizer_alloc(VALUE rclass) {
    FrtTokenStream *ts = frt_letter_tokenizer_new();
    return TypedData_Wrap_Struct(rclass, &frb_token_stream_t, ts);
}

static VALUE frb_letter_tokenizer_init(int argc, VALUE *argv, VALUE self) {
    VALUE rstr;
    rb_scan_args(argc, argv, "1", &rstr);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_token_stream_t, ts);
    ts->reset(ts, rs2s(rstr), rb_enc_get(rstr));
    ts->rts = self;
    return self;
}

/*
 *  call-seq:
 *     WhiteSpaceTokenizer.new -> tokenizer
 *
 *  Create a new WhiteSpaceTokenizer.
 */
static VALUE frb_whitespace_tokenizer_alloc(VALUE rclass) {
    FrtTokenStream *ts = frt_whitespace_tokenizer_new();
    return TypedData_Wrap_Struct(rclass, &frb_token_stream_t, ts);
}

static VALUE frb_whitespace_tokenizer_init(int argc, VALUE *argv, VALUE self) {
    VALUE rstr;
    rb_scan_args(argc, argv, "1", &rstr);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_token_stream_t, ts);
    ts->reset(ts, rs2s(rstr), rb_enc_get(rstr));
    ts->rts = self;
    return self;
}

/*
 *  call-seq:
 *     StandardTokenizer.new -> tokenizer
 *
 *  Create a new StandardTokenizer.
 */
static VALUE frb_standard_tokenizer_alloc(VALUE rclass) {
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    return TypedData_Wrap_Struct(rclass, &frb_token_stream_t, ts);
}

static VALUE frb_standard_tokenizer_init(int argc, VALUE *argv, VALUE self) {
    VALUE rstr;
    rb_scan_args(argc, argv, "1", &rstr);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_token_stream_t, ts);
    ts->reset(ts, rs2s(rstr), rb_enc_get(rstr));
    ts->rts = self;
    return self;
}

/****************************************************************************
 * Filters
 ****************************************************************************/
/*
 *  call-seq:
 *     LowerCaseFilter.new(token_stream) -> token_stream
 *
 *  Create an LowerCaseFilter which normalizes a token's text to
 *  lowercase based on the current locale.
 */

const rb_data_type_t frb_lowercase_filter_t = {
    .wrap_struct_name = "FrbLowercaseFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_tf_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_lowercase_filter_alloc(VALUE rclass) {
    FrtTokenStream *tf = frt_lowercase_filter_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_lowercase_filter_t, tf);
}

static VALUE frb_lowercase_filter_init(VALUE self, VALUE rsub_ts) {
    FrtTokenStream *sub_ts = frb_get_cwrapped_rts(rsub_ts);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_lowercase_filter_t, ts);
    frt_lowercase_filter_init(ts, sub_ts);
    TkFilt(ts)->sub_ts->rts = rsub_ts;
    ts->rts = self;
    return self;
}

/*
 *  call-seq:
 *     HyphenFilter.new(token_stream) -> token_stream
 *
 *  Create an HyphenFilter which filters hyphenated words. The way it works is
 *  by adding both the word concatenated into a single word and split into
 *  multiple words. ie "e-mail" becomes "email" and "e mail". This way a
 *  search for "e-mail", "email" and "mail" will all match. This filter is
 *  used by default by the StandardAnalyzer.
 */

static size_t frb_hyphen_filter_size(const void *p) {
    return sizeof(FrtHyphenFilter);
    (void)p;
}

const rb_data_type_t frb_hyphen_filter_t = {
    .wrap_struct_name = "FrbHyphenFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_hyphen_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_hyphen_filter_alloc(VALUE rclass) {
    FrtTokenStream *hf = frt_hyphen_filter_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_hyphen_filter_t, hf);
}

static VALUE frb_hyphen_filter_init(VALUE self, VALUE rsub_ts) {
    FrtTokenStream *sub_ts = frb_get_cwrapped_rts(rsub_ts);
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_hyphen_filter_t, ts);
    frt_hyphen_filter_init(ts, sub_ts);
    TkFilt(ts)->sub_ts->rts = rsub_ts;
    ts->rts = self;
    return self;
}

/*
 *  call-seq:
 *     StopFilter.new(token_stream) -> token_stream
 *     StopFilter.new(token_stream, ["the", "and", "it"]) -> token_stream
 *
 *  Create an StopFilter which removes *stop-words* from a TokenStream. You can
 *  optionally specify the stopwords you wish to have removed.
 *
 *  token_stream:: TokenStream to be filtered
 *  stop_words::   Array of *stop-words* you wish to be filtered out. This
 *                 defaults to a list of English stop-words. The
 *                 Ferret::Analysis contains a number of stop-word lists.
 */

static size_t frb_stop_filter_size(const void *p) {
    return sizeof(FrtStopFilter);
    (void)p;
}

const rb_data_type_t frb_stop_filter_t = {
    .wrap_struct_name = "FrbStopFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_stop_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_stop_filter_alloc(VALUE rclass) {
    FrtTokenStream *sf = frt_stop_filter_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_stop_filter_t, sf);
}

static VALUE frb_stop_filter_init(int argc, VALUE *argv, VALUE self) {
    VALUE rsub_ts, rstop_words;
    FrtTokenStream *sub_ts;
    FrtTokenStream *ts;
    rb_scan_args(argc, argv, "11", &rsub_ts, &rstop_words);
    sub_ts = frb_get_cwrapped_rts(rsub_ts);

    TypedData_Get_Struct(self, FrtTokenStream, &frb_stop_filter_t, ts);
    if (rstop_words != Qnil) {
        char **stop_words = get_stopwords(rstop_words);
        frt_stop_filter_init(ts, sub_ts);
        frt_stop_filter_set_words(ts, (const char **)stop_words);
        free(stop_words);
    } else {
        frt_stop_filter_init(ts, sub_ts);
    }
    TkFilt(ts)->sub_ts->rts = rsub_ts;

    ts->rts = self;
    return self;
}

static void frb_add_mapping_i(FrtTokenStream *mf, VALUE from, const char *to) {
    switch (TYPE(from)) {
        case T_STRING:
            frt_mapping_filter_add(mf, rs2s(from), to);
            break;
        case T_SYMBOL:
            frt_mapping_filter_add(mf, rb_id2name(SYM2ID(from)), to);
            break;
        default:
            rb_raise(rb_eArgError,
                     "cannot map from %s with MappingFilter",
                     rs2s(rb_obj_as_string(from)));
            break;
    }
}

static int frb_add_mappings_i(VALUE key, VALUE value, VALUE arg) {
    if (key == Qundef) {
        return ST_CONTINUE;
    } else {
        FrtTokenStream *mf = (FrtTokenStream *)arg;
        const char *to;
        switch (TYPE(value)) {
            case T_STRING:
                to = rs2s(value);
                break;
            case T_SYMBOL:
                to = rb_id2name(SYM2ID(value));
                break;
            default:
                rb_raise(rb_eArgError,
                         "cannot map to %s with MappingFilter",
                         rs2s(rb_obj_as_string(key)));
                break;
        }
        if (TYPE(key) == T_ARRAY) {
            int i;
            for (i = RARRAY_LEN(key) - 1; i >= 0; i--) {
                frb_add_mapping_i(mf, RARRAY_PTR(key)[i], to);
            }
        } else {
            frb_add_mapping_i(mf, key, to);
        }
    }
    return ST_CONTINUE;
}


/*
 *  call-seq:
 *     MappingFilter.new(token_stream, mapping) -> token_stream
 *
 *  Create an MappingFilter which maps strings in tokens. This is usually used
 *  to map UTF-8 characters to ASCII characters for easier searching and
 *  better search recall. The mapping is compiled into a Deterministic Finite
 *  Automata so it is super fast. This Filter can therefor be used for
 *  indexing very large datasets. Currently regular expressions are not
 *  supported. If you are really interested in the feature, please contact me
 *  at dbalmain@gmail.com.
 *
 *  token_stream:: TokenStream to be filtered
 *  mapping::      Hash of mappings to apply to tokens. The key can be a
 *                 String or an Array of Strings. The value must be a String
 *
 *  == Example
 *
 *     filt = MappingFilter.new(token_stream,
 *                              {
 *                                ['??','??','??','??','??','??'] => 'a',
 *                                ['??','??','??','??','??','??'] => 'e'
 *                              })
 */

static size_t frb_mapping_filter_size(const void *p) {
    return sizeof(FrtMappingFilter);
    (void)p;
}

const rb_data_type_t frb_mapping_filter_t = {
    .wrap_struct_name = "FrbMappingFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_mapping_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_mapping_filter_alloc(VALUE rclass) {
    FrtTokenStream *mf = frt_mapping_filter_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_mapping_filter_t, mf);
}

static VALUE frb_mapping_filter_init(VALUE self, VALUE rsub_ts, VALUE mapping) {
    FrtTokenStream *ts;
    FrtTokenStream *sub_ts = frb_get_cwrapped_rts(rsub_ts);
    TypedData_Get_Struct(self, FrtTokenStream, &frb_mapping_filter_t, ts);
    frt_mapping_filter_init(ts, sub_ts);
    rb_hash_foreach(mapping, frb_add_mappings_i, (VALUE)ts);
    frt_mulmap_compile(((FrtMappingFilter *)ts)->mapper);
    TkFilt(ts)->sub_ts->rts = rsub_ts;
    ts->rts = self;
    return self;
}

/*
 *  call-seq:
 *     StemFilter.new(token_stream) -> token_stream
 *     StemFilter.new(token_stream,
 *                    algorithm="english",
 *                    encoding="UTF-8") -> token_stream
 *
 *  Create an StemFilter which uses a snowball stemmer (thank you Martin
 *  Porter) to stem words. You can optionally specify the algorithm (default:
 *  "english") and encoding (default: "UTF-8").
 *
 *  token_stream:: TokenStream to be filtered
 *  algorithm::    The algorithm (or language) to use
 */

static size_t frb_stem_filter_size(const void *p) {
    return sizeof(FrtStemFilter);
    (void)p;
}

const rb_data_type_t frb_stem_filter_t = {
    .wrap_struct_name = "FrbStemFilter",
    .function = {
        .dmark = frb_tf_mark,
        .dfree = frb_tf_free,
        .dsize = frb_stem_filter_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_stem_filter_alloc(VALUE rclass) {
    FrtTokenStream *sf = frt_stem_filter_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_stem_filter_t, sf);
}

static VALUE frb_stem_filter_init(int argc, VALUE *argv, VALUE self) {
    VALUE rsub_ts, ralgorithm;
    const char *algorithm = "english";
    FrtTokenStream *sub_ts;
    FrtTokenStream *ts;
    TypedData_Get_Struct(self, FrtTokenStream, &frb_stem_filter_t, ts);

    rb_scan_args(argc, argv, "11", &rsub_ts, &ralgorithm);
    sub_ts = frb_get_cwrapped_rts(rsub_ts);
    if (argc == 2)
        algorithm = rs2s(rb_obj_as_string(ralgorithm));

    frt_stem_filter_init(ts, sub_ts, algorithm);
    TkFilt(ts)->sub_ts->rts = rsub_ts;
    ts->rts = self;
    if (((FrtStemFilter *)ts)->stemmer == NULL) {
        rb_raise(rb_eArgError, "No stemmer could be found for the %s language.", algorithm);
    }
    return self;
}

/****************************************************************************
 *
 * FrtAnalyzer Methods
 *
 ****************************************************************************/

/****************************************************************************
 * CWrappedAnalyzer Methods
 ****************************************************************************/

typedef struct CWrappedAnalyzer {
    FrtAnalyzer super;
    VALUE ranalyzer;
} CWrappedAnalyzer;

#define CWA(analyzer) ((CWrappedAnalyzer *)(analyzer))

static void frb_analyzer_free(void *p) {
    frt_a_deref((FrtAnalyzer *)p);
}

static size_t frb_analyzer_size(const void *p) {
    return sizeof(CWrappedAnalyzer);
    (void)p;
}

const rb_data_type_t frb_analyzer_t = {
    .wrap_struct_name = "FrbAnalyzer",
    .function = {
        .dmark = NULL,
        .dfree = frb_analyzer_free,
        .dsize = frb_analyzer_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static void cwa_destroy_i(FrtAnalyzer *a) {
    rb_hash_delete(object_space, ((VALUE)a)|1);
    free(a);
}

static FrtTokenStream *cwa_get_ts(FrtAnalyzer *a, ID field, char *text, rb_encoding *encoding) {
    VALUE rstr = rb_str_new_cstr(text);
    rb_enc_associate(rstr, encoding);
    VALUE rts = rb_funcall(CWA(a)->ranalyzer, id_token_stream, 2, rb_str_new_cstr(rb_id2name(field)), rstr);
    return frb_get_cwrapped_rts(rts);
}

FrtAnalyzer *frb_get_cwrapped_analyzer(VALUE ranalyzer) {
    FrtAnalyzer *a = NULL;
    if (frb_is_cclass(ranalyzer) && DATA_PTR(ranalyzer)) {
        // TypedData_Get_Struct(ranalyzer, FrtAnalyzer, &frb_analyzer_t, a);
        a = DATA_PTR(ranalyzer);
        FRT_REF(a);
    } else {
        a = (FrtAnalyzer *)frt_ecalloc(sizeof(CWrappedAnalyzer));
        a->destroy_i = &cwa_destroy_i;
        a->get_ts    = &cwa_get_ts;
        a->ref_cnt   = 1;
        CWA(a)->ranalyzer = ranalyzer;
        /* prevent from being garbage collected */
        rb_hash_aset(object_space, ((VALUE)a)|1, ranalyzer);
    }
    return a;
}

VALUE frb_get_analyzer(FrtAnalyzer *a) {
    VALUE self = Qnil;
    if (a) {
        self = a->ranalyzer;
        if (self == 0 || self == Qnil) {
            self = TypedData_Wrap_Struct(cAnalyzer, &frb_analyzer_t, a);
            FRT_REF(a);
            a->ranalyzer = self;
        }
    }
    return self;
}

VALUE get_rb_ts_from_a(FrtAnalyzer *a, VALUE rfield, VALUE rstring) {
    FrtTokenStream *ts = frt_a_get_ts(a, frb_field(rfield), rs2s(rstring), rb_enc_get(rstring));
    /* Make sure that there is no entry already */
    return get_rb_token_stream(ts);
}

/*
 *  call-seq:
 *     analyzer.token_stream(field_name, input) -> token_stream
 *
 *  Create a new TokenStream to tokenize +input+. The TokenStream created may
 *  also depend on the +field_name+. Although this parameter is typically
 *  ignored.
 *
 *  field_name:: name of the field to be tokenized
 *  input::      data from the field to be tokenized
 */
static VALUE frb_analyzer_token_stream(VALUE self, VALUE rfield, VALUE rstring) {
    /* NOTE: Any changes made to this method may also need to be applied to
     * frb_re_analyzer_token_stream */
    FrtAnalyzer *a = DATA_PTR(self);
    StringValue(rstring);
    return get_rb_ts_from_a(a, rfield, rstring);
}

#define GET_LOWER(dflt) \
    bool lower;\
VALUE rlower;\
rb_scan_args(argc, argv, "01", &rlower);\
lower = (argc ? RTEST(rlower) : dflt)

static VALUE frb_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a = frt_letter_analyzer_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_analyzer_t, a);
}

static VALUE frb_analyzer_init(int argc, VALUE *argv, VALUE self) {
    FrtAnalyzer *a;
    GET_LOWER(true);
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_analyzer_t, a);
    frt_letter_analyzer_init(a, lower);
    a->ranalyzer = self;
    return self;
}


/*****************************************************************************/
/*** WhiteSpaceAnalyzer ******************************************************/
/*****************************************************************************/
/*
 *  call-seq:
 *     WhiteSpaceAnalyzer.new(lower = false) -> analyzer
 *
 *  Create a new WhiteSpaceAnalyzer which downcases tokens by default but can
 *  optionally leave case as is. Lowercasing will be done based on the current
 *  locale.
 *
 *  lower:: set to false if you don't want the field's tokens to be downcased
 */

static VALUE frb_whitespace_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a = frt_whitespace_analyzer_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_analyzer_t, a);
}

static VALUE frb_whitespace_analyzer_init(int argc, VALUE *argv, VALUE self) {
    FrtAnalyzer *a;
    GET_LOWER(false);
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_analyzer_t, a);
    frt_whitespace_analyzer_init(a, lower);
    a->ranalyzer = self;
    return self;
}

/*
 *  call-seq:
 *     LetterAnalyzer.new(lower = true) -> analyzer
 *
 *  Create a new LetterAnalyzer which downcases tokens by default but can
 *  optionally leave case as is. Lowercasing will be done based on the current
 *  locale.
 *
 *  lower:: set to false if you don't want the field's tokens to be downcased
 */

static VALUE frb_letter_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a = frt_letter_analyzer_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_analyzer_t, a);
}

static VALUE frb_letter_analyzer_init(int argc, VALUE *argv, VALUE self) {
    FrtAnalyzer *a;
    GET_LOWER(true);
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_analyzer_t, a);
    frt_letter_analyzer_init(a, lower);
    a->ranalyzer = self;
    return self;
}

static VALUE
get_rstopwords(const char **stop_words)
{
    char **w = (char **)stop_words;
    VALUE rstopwords = rb_ary_new();

    while (*w) {
        rb_ary_push(rstopwords, rb_str_new2(*w));
        w++;
    }
    return rstopwords;
}

/*
 *  call-seq:
 *     StandardAnalyzer.new(stop_words = FRT_FULL_ENGLISH_STOP_WORDS, lower=true)
 *     -> analyzer
 *
 *  Create a new StandardAnalyzer which downcases tokens by default but can
 *  optionally leave case as is. Lowercasing will be done based on the current
 *  locale. You can also set the list of stop-words to be used by the
 *  StopFilter.
 *
 *  lower::      set to false if you don't want the field's tokens to be downcased
 *  stop_words:: list of stop-words to pass to the StopFilter
 */
static VALUE frb_standard_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a = frt_standard_analyzer_alloc();
    return TypedData_Wrap_Struct(rclass, &frb_analyzer_t, a);
}

static VALUE frb_standard_analyzer_init(int argc, VALUE *argv, VALUE self) {
    bool lower;
    VALUE rlower, rstop_words;
    FrtAnalyzer *a;
    rb_scan_args(argc, argv, "02", &rstop_words, &rlower);
    lower = ((rlower == Qnil) ? true : RTEST(rlower));
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_analyzer_t, a);
    if (rstop_words != Qnil) {
        char **stop_words = get_stopwords(rstop_words);
        frt_standard_analyzer_init(a, lower, (const char **)stop_words);
        free(stop_words);
    } else {
        frt_standard_analyzer_init(a, lower, FRT_FULL_ENGLISH_STOP_WORDS);
    }
    a->ranalyzer = self;
    return self;
}

static void frb_h_mark_values_i(void *key, void *value, void *arg) {
    if (((FrtAnalyzer *)value)->ranalyzer)
        rb_gc_mark(((FrtAnalyzer *)value)->ranalyzer);
}

static void frb_pfa_mark(void *p) {
    if (PFA(p)->default_a && PFA(p)->default_a->ranalyzer)
        rb_gc_mark(PFA(p)->default_a->ranalyzer);
    if (PFA(p)->dict)
        frt_h_each(PFA(p)->dict, &frb_h_mark_values_i, NULL);
}

/*** PerFieldAnalyzer ***/

/*
 *  call-seq:
 *     PerFieldAnalyzer.new(default_analyzer) -> analyzer
 *
 *  Create a new PerFieldAnalyzer specifying the default analyzer to use on
 *  all fields that are set specifically.
 *
 *  default_analyzer:: analyzer to be used on fields that aren't otherwise
 *                     specified
 */

static size_t frb_per_field_analyzer_size(const void *p) {
    return sizeof(FrtPerFieldAnalyzer);
    (void)p;
}

const rb_data_type_t frb_per_field_analyzer_t = {
    .wrap_struct_name = "FrbPerFieldAnalyzer",
    .function = {
        .dmark = frb_pfa_mark,
        .dfree = frb_analyzer_free,
        .dsize = frb_per_field_analyzer_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_per_field_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a = frt_per_field_analyzer_alloc();
    PFA(a)->default_a = NULL;
    PFA(a)->dict = NULL;
    return TypedData_Wrap_Struct(rclass, &frb_per_field_analyzer_t, a);
}

static VALUE frb_per_field_analyzer_init(VALUE self, VALUE ranalyzer) {
    FrtAnalyzer *def = frb_get_cwrapped_analyzer(ranalyzer);
    FrtAnalyzer *a;
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_per_field_analyzer_t, a);
    frt_per_field_analyzer_init(a, def);
    a->ranalyzer = self;
    return self;
}

/*
 *  call-seq:
 *     per_field_analyzer.add_field(field_name, default_analyzer) -> self
 *     per_field_analyzer[field_name] = default_analyzer -> self
 *
 *  Set the analyzer to be used on field +field_name+. Note that field_name
 *  should be a symbol.
 *
 *  field_name:: field we wish to set the analyzer for
 *  analyzer::   analyzer to be used on +field_name+
 */
static VALUE frb_per_field_analyzer_add_field(VALUE self, VALUE rfield, VALUE ranalyzer) {
    FrtAnalyzer *pfa, *a;
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_per_field_analyzer_t, pfa);
    a = frb_get_cwrapped_analyzer(ranalyzer);

    frt_pfa_add_field(pfa, frb_field(rfield), a);
    return self;
}

/*
 *  call-seq:
 *     analyzer.token_stream(field_name, input) -> token_stream
 *
 *  Create a new TokenStream to tokenize +input+. The TokenStream created will
 *  also depend on the +field_name+ in the case of the PerFieldAnalyzer.
 *
 *  field_name:: name of the field to be tokenized
 *  input::      data from the field to be tokenized
 */
static VALUE frb_pfa_analyzer_token_stream(VALUE self, VALUE rfield, VALUE rstring) {
    FrtAnalyzer *pfa, *a;
    ID field = frb_field(rfield);
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_per_field_analyzer_t, pfa);

    StringValue(rstring);
    a = (FrtAnalyzer *)frt_h_get(PFA(pfa)->dict, (void *)field);
    if (a == NULL) {
        a = PFA(pfa)->default_a;
    }
    if (a->get_ts == cwa_get_ts) {
        VALUE rstr = rb_str_new_cstr(rs2s(rstring));
        rb_enc_associate(rstr, rb_enc_get(rstring));
        return rb_funcall(CWA(a)->ranalyzer, id_token_stream, 2,
                          rb_str_new_cstr(rb_id2name(field)), rstr);
    } else {
        return get_rb_ts_from_a(a, rfield, rstring);
    }
}

/*** RegExpAnalyzer ***/

static void frb_re_analyzer_mark(void *p) {
    if (((FrtAnalyzer *)p)->current_ts && ((FrtAnalyzer *)p)->current_ts->rts)
        rb_gc_mark(((FrtAnalyzer *)p)->current_ts->rts);
}

static void re_analyzer_destroy_i(FrtAnalyzer *a) {
    frt_ts_deref(a->current_ts);
    free(a);
}

const rb_data_type_t frb_reg_exp_analyzer_t = {
    .wrap_struct_name = "FrbRegExpAnalyzer",
    .function = {
        .dmark = frb_re_analyzer_mark,
        .dfree = frb_analyzer_free,
        .dsize = frb_analyzer_size,
        .dcompact = 0,
        .reserved = {0},
    },
    .parent = 0,
    .data = 0,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_reg_exp_analyzer_alloc(VALUE rclass) {
    FrtAnalyzer *a;
    return TypedData_Make_Struct(rclass, FrtAnalyzer, &frb_reg_exp_analyzer_t, a);
}

/*
 *  call-seq:
 *     RegExpAnalyzer.new(reg_exp, lower = true) -> analyzer
 *
 *  Create a new RegExpAnalyzer which will create tokenizers based on the
 *  regular expression and lowercasing if required.
 *
 *  reg_exp:: the token matcher for the tokenizer to use
 *  lower::   set to false if you don't want to downcase the tokens
 */
static VALUE frb_re_analyzer_init(int argc, VALUE *argv, VALUE self) {
    VALUE lower, rets, regex, proc;
    FrtAnalyzer *a;
    FrtTokenStream *ts;
    rb_scan_args(argc, argv, "02&", &regex, &lower, &proc);

    ts = rets_new(Qnil, regex, proc);
    rets = TypedData_Wrap_Struct(cRegExpTokenizer, &frb_reg_exp_token_stream_t, ts);
    ts->rts = rets;

    if (lower != Qfalse) {
        rets = frb_lowercase_filter_init(frb_lowercase_filter_alloc(cLowerCaseFilter), rets);
        ts = DATA_PTR(rets);
        ts->rts = rets;
    }
    FRT_REF(ts);

    TypedData_Get_Struct(self, FrtAnalyzer, &frb_reg_exp_analyzer_t, a);
    frt_analyzer_init(a, ts, &re_analyzer_destroy_i, NULL);
    a->ranalyzer = self;
    return self;
}

/*
 *  call-seq:
 *     analyzer.token_stream(field_name, input) -> token_stream
 *
 *  Create a new TokenStream to tokenize +input+. The TokenStream created may
 *  also depend on the +field_name+. Although this parameter is typically
 *  ignored.
 *
 *  field_name:: name of the field to be tokenized
 *  input::      data from the field to be tokenized
 */
static VALUE frb_re_analyzer_token_stream(VALUE self, VALUE rfield, VALUE rtext) {
    FrtTokenStream *ts;
    FrtAnalyzer *a;
    TypedData_Get_Struct(self, FrtAnalyzer, &frb_reg_exp_analyzer_t, a);

    ts = frt_a_get_ts(a, frb_field(rfield), rs2s(rtext), rb_enc_get(rtext));

    return get_rb_token_stream(ts);
}

/****************************************************************************
 *
 * Init Functions
 *
 ****************************************************************************/

/*
 *  Document-class: Ferret::Analysis::Token
 *
 *  == Summary
 *
 *  A Token is an occurrence of a term from the text of a field.  It consists
 *  of a term's text and the start and end offset of the term in the text of
 *  the field;
 *
 *  The start and end offsets permit applications to re-associate a token with
 *  its source text, e.g., to display highlighted query terms in a document
 *  browser, or to show matching text fragments in a KWIC (KeyWord In Context)
 *  display, etc.
 *
 *  === Attributes
 *
 *  text::  the terms text which may have been modified by a Token Filter or
 *          Tokenizer from the text originally found in the document
 *  start:: is the position of the first character corresponding to
 *          this token in the source text
 *  end::   is equal to one greater than the position of the last
 *          character corresponding of this token Note that the
 *          difference between @end_offset and @start_offset may not be
 *          equal to @text.length(), as the term text may have been
 *          altered by a stemmer or some other filter.
 */
static void Init_Token(void) {
    cToken = rb_define_class_under(mAnalysis, "Token", rb_cObject);
    rb_define_alloc_func(cToken, frb_token_alloc);
    rb_include_module(cToken, rb_mComparable);

    rb_define_method(cToken, "initialize",  frb_token_init, -1);
    rb_define_method(cToken, "<=>",         frb_token_cmp, 1);
    rb_define_method(cToken, "text",        frb_token_get_text, 0);
    rb_define_method(cToken, "text=",       frb_token_set_text, 1);
    rb_define_method(cToken, "start",       frb_token_get_start_offset, 0);
    rb_define_method(cToken, "start=",      frb_token_set_start_offset, 1);
    rb_define_method(cToken, "end",         frb_token_get_end_offset, 0);
    rb_define_method(cToken, "end=",        frb_token_set_end_offset, 1);
    rb_define_method(cToken, "pos_inc",     frb_token_get_pos_inc, 0);
    rb_define_method(cToken, "pos_inc=",    frb_token_set_pos_inc, 1);
    rb_define_method(cToken, "to_s",        frb_token_to_s, 0);
}

/*
 *  Document-class: Ferret::Analysis::TokenStream
 *
 *  == Summary
 *
 *  A TokenStream enumerates the sequence of tokens, either from
 *  fields of a document or from query text.
 *
 *  This is an abstract class.  Concrete subclasses are:
 *
 *  Tokenizer::   a TokenStream whose input is a string
 *  TokenFilter:: a TokenStream whose input is another TokenStream
 */
static void Init_TokenStream(void) {
    cTokenStream = rb_define_class_under(mAnalysis, "TokenStream", rb_cObject);
    frb_mark_cclass(cTokenStream);
    rb_define_method(cTokenStream, "next", frb_ts_next, 0);
    rb_define_method(cTokenStream, "text=", frb_ts_set_text, 1);
    rb_define_method(cTokenStream, "text", frb_ts_get_text, 0);
}

/*
 *  Document-class: Ferret::Analysis::LetterTokenizer
 *
 *  == Summary
 *
 *  A LetterTokenizer is a tokenizer that divides text at non-letters. That is
 *  to say, it defines tokens as maximal strings of adjacent letters, as
 *  defined by the regular expression _/[[:alpha:]]+/_ where [:alpha] matches
 *  all characters in your local locale.
 *
 *  === Example
 *
 *    "Dave's r??sum??, at http://www.davebalmain.com/ 1234"
 *      => ["Dave", "s", "r??sum??", "at", "http", "www", "davebalmain", "com"]
 */
static void Init_LetterTokenizer(void) {
    cLetterTokenizer = rb_define_class_under(mAnalysis, "LetterTokenizer", cTokenStream);
    frb_mark_cclass(cLetterTokenizer);
    rb_define_alloc_func(cLetterTokenizer, frb_letter_tokenizer_alloc);
    rb_define_method(cLetterTokenizer, "initialize", frb_letter_tokenizer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::WhiteSpaceTokenizer
 *
 *  == Summary
 *
 *  A WhiteSpaceTokenizer is a tokenizer that divides text at white-space.
 *  Adjacent sequences of non-WhiteSpace characters form tokens.
 *
 *  === Example
 *
 *    "Dave's r??sum??, at http://www.davebalmain.com/ 1234"
 *      => ["Dave's", "r??sum??,", "at", "http://www.davebalmain.com", "1234"]
 */
static void Init_WhiteSpaceTokenizer(void) {
    cWhiteSpaceTokenizer = rb_define_class_under(mAnalysis, "WhiteSpaceTokenizer", cTokenStream);
    frb_mark_cclass(cWhiteSpaceTokenizer);
    rb_define_alloc_func(cWhiteSpaceTokenizer, frb_whitespace_tokenizer_alloc);
    rb_define_method(cWhiteSpaceTokenizer, "initialize", frb_whitespace_tokenizer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::StandardTokenizer
 *
 *  == Summary
 *
 *  The standard tokenizer is an advanced tokenizer which tokenizes most
 *  words correctly as well as tokenizing things like email addresses, web
 *  addresses, phone numbers, etc.
 *
 *  === Example
 *
 *    "Dave's r??sum??, at http://www.davebalmain.com/ 1234"
 *      => ["Dave's", "r??sum??", "at", "http://www.davebalmain.com", "1234"]
 */
static void Init_StandardTokenizer(void) {
    cStandardTokenizer = rb_define_class_under(mAnalysis, "StandardTokenizer", cTokenStream);
    frb_mark_cclass(cStandardTokenizer);
    rb_define_alloc_func(cStandardTokenizer, frb_standard_tokenizer_alloc);
    rb_define_method(cStandardTokenizer, "initialize", frb_standard_tokenizer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::RegExpTokenizer
 *
 *  == Summary
 *
 *  A tokenizer that recognizes tokens based on a regular expression passed to
 *  the constructor. Most possible tokenizers can be created using this class.
 *
 *  === Example
 *
 *  Below is an example of a simple implementation of a LetterTokenizer using
 *  an RegExpTokenizer. Basically, a token is a sequence of alphabetic
 *  characters separated by one or more non-alphabetic characters.
 *
 *    # of course you would add more than just ??
 *    RegExpTokenizer.new(input, /[[:alpha:]??]+/)
 *
 *    "Dave's r??sum??, at http://www.davebalmain.com/ 1234"
 *      => ["Dave", "s", "r??sum??", "at", "http", "www", "davebalmain", "com"]
 */
static void Init_RegExpTokenizer(void) {
    cRegExpTokenizer = rb_define_class_under(mAnalysis, "RegExpTokenizer", cTokenStream);
    frb_mark_cclass(cRegExpTokenizer);
    rtoken_re = rb_reg_new(TOKEN_RE, strlen(TOKEN_RE), 0);
    rb_define_const(cRegExpTokenizer, "REGEXP", rtoken_re);
    rb_define_alloc_func(cRegExpTokenizer, frb_reg_exp_tokenizer_alloc);
    rb_define_method(cRegExpTokenizer, "initialize", frb_rets_init, -1);
    rb_define_method(cRegExpTokenizer, "text=", frb_rets_set_text, 1);
    rb_define_method(cRegExpTokenizer, "text", frb_rets_get_text, 0);
}

/***************/
/*** Filters ***/
/***************/
/*
 *  Document-class: Ferret::Analysis::LowerCaseFilter
 *
 *  == Summary
 *
 *  LowerCaseFilter normalizes a token's text to lowercase based on the
 *  current locale.
 *
 *  === Example
 *
 *    ["One", "TWO", "three", "R??SUM??"] => ["one", "two", "three", "r??sum??"]
 *
 */
static void Init_LowerCaseFilter(void) {
    cLowerCaseFilter = rb_define_class_under(mAnalysis, "LowerCaseFilter", cTokenStream);
    frb_mark_cclass(cLowerCaseFilter);
    rb_define_alloc_func(cLowerCaseFilter, frb_lowercase_filter_alloc);
    rb_define_method(cLowerCaseFilter, "initialize", frb_lowercase_filter_init, 1);
}

/*
 *  Document-class: Ferret::Analysis::HyphenFilter
 *
 *  == Summary
 *
 *  HyphenFilter filters hyphenated words by adding both the word concatenated
 *  into a single word and split into multiple words. ie "e-mail" becomes
 *  "email" and "e mail". This way a search for "e-mail", "email" and "mail"
 *  will all match. This filter is used by default by the StandardAnalyzer.
 *
 *  === Example
 *
 *    ["e-mail", "set-up"] => ["email", "e", "mail", "setup", "set", "up"]
 *
 */
static void Init_HyphenFilter(void) {
    cHyphenFilter = rb_define_class_under(mAnalysis, "HyphenFilter", cTokenStream);
    frb_mark_cclass(cHyphenFilter);
    rb_define_alloc_func(cHyphenFilter, frb_hyphen_filter_alloc);
    rb_define_method(cHyphenFilter, "initialize", frb_hyphen_filter_init, 1);
}

/*
 *  Document-class: Ferret::Analysis::MappingFilter
 *
 *  == Summary
 *
 *  A MappingFilter maps strings in tokens. This is usually used to map UTF-8
 *  characters to ASCII characters for easier searching and better search
 *  recall. The mapping is compiled into a Deterministic Finite Automata so it
 *  is super fast. This Filter can therefor be used for indexing very large
 *  datasets. Currently regular expressions are not supported. If you are
 *  really interested in the feature, please contact me at dbalmain@gmail.com.
 *
 *  == Example
 *
 *     mapping = {
 *       ['??','??','??','??','??','??','??','??']         => 'a',
 *       '??'                                       => 'ae',
 *       ['??','??']                                 => 'd',
 *       ['??','??','??','??','??']                     => 'c',
 *       ['??','??','??','??','??','??','??','??','??',]    => 'e',
 *       ['??']                                     => 'f',
 *       ['??','??','??','??']                         => 'g',
 *       ['??','??']                                 => 'h',
 *       ['??','??','??','??','??','??','??','??']         => 'i',
 *       ['??','??','??','??']                         => 'j',
 *       ['??','??']                                 => 'k',
 *       ['??','??','??','??','??']                     => 'l',
 *       ['??','??','??','??','??','??']                 => 'n',
 *       ['??','??','??','??','??','??','??','??','??','??'] => 'o',
 *       ['??']                                     => 'oek',
 *       ['??']                                     => 'q',
 *       ['??','??','??']                             => 'r',
 *       ['??','??','??','??','??']                     => 's',
 *       ['??','??','??','??']                         => 't',
 *       ['??','??','??','??','??','??','??','??','??','??'] => 'u',
 *       ['??']                                     => 'w',
 *       ['??','??','??']                             => 'y',
 *       ['??','??','??']                             => 'z'
 *     }
 *     filt = MappingFilter.new(token_stream, mapping)
 */
static void Init_MappingFilter(void) {
    cMappingFilter = rb_define_class_under(mAnalysis, "MappingFilter", cTokenStream);
    frb_mark_cclass(cMappingFilter);
    rb_define_alloc_func(cMappingFilter, frb_mapping_filter_alloc);
    rb_define_method(cMappingFilter, "initialize", frb_mapping_filter_init, 2);
}

/*
 *  Document-class: Ferret::Analysis::StopFilter
 *
 *  == Summary
 *
 *  A StopFilter filters *stop-words* from a TokenStream. Stop-words are words
 *  that you don't wish to be index. Usually they will be common words like
 *  "the" and "and" although you can specify whichever words you want.
 *
 *  === Example
 *
 *    ["the", "pig", "and", "whistle"] => ["pig", "whistle"]
 */
static void Init_StopFilter(void) {
    cStopFilter = rb_define_class_under(mAnalysis, "StopFilter", cTokenStream);
    frb_mark_cclass(cStopFilter);
    rb_define_alloc_func(cStopFilter, frb_stop_filter_alloc);
    rb_define_method(cStopFilter, "initialize", frb_stop_filter_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::StemFilter
 *
 *  == Summary
 *
 *  A StemFilter takes a term and transforms the term as per the SnowBall
 *  stemming algorithm.  Note: the input to the stemming filter must already
 *  be in lower case, so you will need to use LowerCaseFilter or lowercasing
 *  Tokenizer further down the Tokenizer chain in order for this to work
 *  properly! Stemmers work on UTF-8 encoding only.
 *
 *  === Available algorithms
 *
 *    Algorithm       Algorithm Pseudonyms
 *    ------------------------------------------
 *     "arabic"      | "ar", "ara"
 *     "armenian"    | "arm", "hy", "hye"
 *     "basque"      | "baq", "eu", "eus"
 *     "catalan"     | "ca", "cat"
 *     "danish"      | "da", "dan"
 *     "dutch"       | "dut", "nl", "nld"
 *     "english"     | "en", "eng"
 *     "finnish"     | "fi", "fin"
 *     "french"      | "fr", "fra", "fre"
 *     "german"      | "de", "deu", "ge", "ger"
 *     "greek"       | "el", "ell", "gre"
 *     "hindi"       | "hi", "hin"
 *     "hungarian"   | "hu", "hun"
 *     "indonesian"  | "id", "ind"
 *     "italian"     | "it", "ita"
 *     "irish"       | "ga", "gle"
 *     "lithuanian"  | "lit"
 *     "nepali"      | "ne", "nep"
 *     "norwegian"   | "nl", "no", "nor"
 *     "porter"      |
 *     "portuguese"  | "por", "pt"
 *     "romanian"    | "ro", "ron", "rum"
 *     "russian"     | "ru", "rus"
 *     "serbian"     | "sr", "srp"
 *     "spanish"     | "es", "esl", "spa"
 *     "swedish"     | "sv", "swe"
 *     "tamil"       | "ta", "tam"
 *     "turkish"     | "tr", "tur"
 *     "yiddish"     | "yi", "yid"
 *
 *
 *  === New Stemmers
 *
 *  The following stemmers have recently benn added. Please try them out;
 *
 *    * Hungarian
 *    * Romanian
 *    * Turkish
 *
 *  === Example
 *
 *  To use this filter with other analyzers, you'll want to write an Analyzer
 *  class that sets up the TokenStream chain as you want it.  To use this with
 *  a lowercasing Tokenizer, for example, you'd write an analyzer like this:
 *
 *    def MyAnalyzer < Analyzer
 *      def token_stream(field, str)
 *        return StemFilter.new(LowerCaseFilter.new(StandardTokenizer.new(str)))
 *      end
 *    end
 *
 *    "debate debates debated debating debater"
 *      => ["debat", "debat", "debat", "debat", "debat"]
 *
 *  === Attributes
 *
 *  token_stream:: TokenStream to be filtered
 *  algorithm::    The algorithm (or language) to use (default: "english")
 *  encoding::     The encoding of the data (default: "UTF-8")
 */
static void Init_StemFilter(void) {
    cStemFilter = rb_define_class_under(mAnalysis, "StemFilter", cTokenStream);
    frb_mark_cclass(cStemFilter);
    rb_define_alloc_func(cStemFilter, frb_stem_filter_alloc);
    rb_define_method(cStemFilter, "initialize", frb_stem_filter_init, -1);
}

/*************************/
/*** * * Analyzers * * ***/
/*************************/

/*
 *  Document-class: Ferret::Analysis::Analyzer
 *
 *  == Summary
 *
 *  An FrtAnalyzer builds TokenStreams, which analyze text.  It thus represents
 *  a policy for extracting index terms from text.
 *
 *  Typical implementations first build a Tokenizer, which breaks the stream
 *  of characters from the Reader into raw Tokens. One or more TokenFilters
 *  may then be applied to the output of the Tokenizer.
 *
 *  The default FrtAnalyzer just creates a LowerCaseTokenizer which converts
 *  all text to lowercase tokens. See LowerCaseTokenizer for more details.
 *
 *  === Example
 *
 *  To create your own custom FrtAnalyzer you simply need to implement a
 *  token_stream method which takes the field name and the data to be
 *  tokenized as parameters and returns a TokenStream. Most analyzers
 *  typically ignore the field name.
 *
 *  Here we'll create a StemmingAnalyzer;
 *
 *    def MyAnalyzer < Analyzer
 *      def token_stream(field, str)
 *        return StemFilter.new(LowerCaseFilter.new(StandardTokenizer.new(str)))
 *      end
 *    end
 */
static void Init_Analyzer(void) {
    cAnalyzer = rb_define_class_under(mAnalysis, "Analyzer", rb_cObject);
    frb_mark_cclass(cAnalyzer);
    rb_define_alloc_func(cAnalyzer, frb_analyzer_alloc);
    rb_define_method(cAnalyzer, "initialize", frb_analyzer_init, -1);
    rb_define_method(cAnalyzer, "token_stream", frb_analyzer_token_stream, 2);
}

/*
 *  Document-class: Ferret::Analysis::LetterAnalyzer
 *
 *  == Summary
 *
 *  A LetterAnalyzer creates a TokenStream that splits the input up into
 *  maximal strings of characters as recognized by the current encoding. If
 *  implemented in Ruby it would look like;
 *
 *    class LetterAnalyzer
 *      def initialize(lower = true)
 *        @lower = lower
 *      end
 *
 *      def token_stream(field, str)
 *        return LetterTokenizer.new(str, @lower)
 *      end
 *    end
 *
 *  As you can see it makes use of the LetterTokenizer.
 */
static void Init_LetterAnalyzer(void) {
    cLetterAnalyzer = rb_define_class_under(mAnalysis, "LetterAnalyzer", cAnalyzer);
    frb_mark_cclass(cLetterAnalyzer);
    rb_define_alloc_func(cLetterAnalyzer, frb_letter_analyzer_alloc);
    rb_define_method(cLetterAnalyzer, "initialize", frb_letter_analyzer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::WhiteSpaceAnalyzer
 *
 *  == Summary
 *
 *  The WhiteSpaceAnalyzer recognizes tokens as maximal strings of
 *  non-whitespace characters. If implemented in Ruby the WhiteSpaceAnalyzer
 *  would look like;
 *
 *    class WhiteSpaceAnalyzer
 *      def initialize(lower = true)
 *        @lower = lower
 *      end
 *
 *      def token_stream(field, str)
 *        return WhiteSpaceTokenizer.new(str, @lower)
 *      end
 *    end
 *
 *  As you can see it makes use of the WhiteSpaceTokenizer.
 */
static void Init_WhiteSpaceAnalyzer(void) {
    cWhiteSpaceAnalyzer = rb_define_class_under(mAnalysis, "WhiteSpaceAnalyzer", cAnalyzer);
    frb_mark_cclass(cWhiteSpaceAnalyzer);
    rb_define_alloc_func(cWhiteSpaceAnalyzer, frb_whitespace_analyzer_alloc);
    rb_define_method(cWhiteSpaceAnalyzer, "initialize", frb_whitespace_analyzer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::StandardAnalyzer
 *
 *  == Summary
 *
 *  The StandardAnalyzer is the most advanced of the available analyzers. If
 *  it were implemented in Ruby it would look like this;
 *
 *    class StandardAnalyzer
 *      def initialize(stop_words = FRT_FULL_ENGLISH_STOP_WORDS, lower = true)
 *        @lower = lower
 *        @stop_words = stop_words
 *      end
 *
 *      def token_stream(field, str)
 *        ts = StandardTokenizer.new(str)
 *        ts = LowerCaseFilter.new(ts) if @lower
 *        ts = StopFilter.new(ts, @stop_words)
 *        ts = HyphenFilter.new(ts)
 *      end
 *    end
 *
 *  As you can see it makes use of the StandardTokenizer and you can also add
 *  your own list of stopwords if you wish.
 */
static void Init_StandardAnalyzer(void) {
    cStandardAnalyzer = rb_define_class_under(mAnalysis, "StandardAnalyzer", cAnalyzer);
    frb_mark_cclass(cStandardAnalyzer);
    rb_define_alloc_func(cStandardAnalyzer, frb_standard_analyzer_alloc);
    rb_define_method(cStandardAnalyzer, "initialize", frb_standard_analyzer_init, -1);
}

/*
 *  Document-class: Ferret::Analysis::PerFieldAnalyzer
 *
 *  == Summary
 *
 *  The PerFieldAnalyzer is for use when you want to analyze different fields
 *  with different analyzers. With the PerFieldAnalyzer you can specify how
 *  you want each field analyzed.
 *
 *  === Example
 *
 *    # Create a new PerFieldAnalyzer which uses StandardAnalyzer by default
 *    pfa = PerFieldAnalyzer.new(StandardAnalyzer.new())
 *
 *    # Use the WhiteSpaceAnalyzer with no lowercasing on the :title field
 *    pfa[:title] = WhiteSpaceAnalyzer.new(false)
 *
 *    # Use a custom analyzer on the :created_at field
 *    pfa[:created_at] = DateAnalyzer.new
 */
static void Init_PerFieldAnalyzer(void) {
    cPerFieldAnalyzer = rb_define_class_under(mAnalysis, "PerFieldAnalyzer", cAnalyzer);
    frb_mark_cclass(cPerFieldAnalyzer);
    rb_define_alloc_func(cPerFieldAnalyzer, frb_per_field_analyzer_alloc);
    rb_define_method(cPerFieldAnalyzer, "initialize", frb_per_field_analyzer_init, 1);
    rb_define_method(cPerFieldAnalyzer, "add_field", frb_per_field_analyzer_add_field, 2);
    rb_define_method(cPerFieldAnalyzer, "[]=", frb_per_field_analyzer_add_field, 2);
    rb_define_method(cPerFieldAnalyzer, "token_stream", frb_pfa_analyzer_token_stream, 2);
}

/*
 *  Document-class: Ferret::Analysis::RegExpAnalyzer
 *
 *  == Summary
 *
 *  Using a RegExpAnalyzer is a simple way to create a custom analyzer. If
 *  implemented in Ruby it would look like this;
 *
 *    class RegExpAnalyzer
 *      def initialize(reg_exp, lower = true)
 *        @lower = lower
 *        @reg_exp = reg_exp
 *      end
 *
 *      def token_stream(field, str)
 *        if @lower
 *          return LowerCaseFilter.new(RegExpTokenizer.new(str, reg_exp))
 *        else
 *          return RegExpTokenizer.new(str, reg_exp)
 *        end
 *      end
 *    end
 *
 *  === Example
 *
 *    csv_analyzer = RegExpAnalyzer.new(/[^,]+/, false)
 */
static void Init_RegExpAnalyzer(void) {
    cRegExpAnalyzer = rb_define_class_under(mAnalysis, "RegExpAnalyzer", cAnalyzer);
    frb_mark_cclass(cRegExpAnalyzer);
    rb_define_alloc_func(cRegExpAnalyzer, frb_reg_exp_analyzer_alloc);
    rb_define_method(cRegExpAnalyzer, "initialize", frb_re_analyzer_init, -1);
    rb_define_method(cRegExpAnalyzer, "token_stream", frb_re_analyzer_token_stream, 2);
}

/* rdoc hack
extern VALUE mFerret = rb_define_module("Ferret");
*/

/*
 *  Document-module: Ferret::Analysis
 *
 *  == Summary
 *
 *  The Analysis module contains all the classes used to analyze and tokenize
 *  the data to be indexed. There are three main classes you need to know
 *  about when dealing with analysis; Analyzer, TokenStream and Token.
 *
 *  == Classes
 *
 *  === Analyzer
 *
 *  Analyzers handle all of your tokenizing needs. You pass an FrtAnalyzer to the
 *  indexing class when you create it and it will create the TokenStreams
 *  necessary to tokenize the fields in the documents. Most of the time you
 *  won't need to worry about TokenStreams and Tokens, one of the Analyzers
 *  distributed with Ferret will do exactly what you need. Otherwise you'll
 *  need to implement a custom analyzer.
 *
 *  === TokenStream
 *
 *  A TokenStream is an enumeration of Tokens. There are two standard types of
 *  TokenStream; Tokenizer and TokenFilter. A Tokenizer takes a String and
 *  turns it into a list of Tokens. A TokenFilter takes another TokenStream
 *  and post-processes the Tokens. You can chain as many TokenFilters together
 *  as you like but they always need to finish with a Tokenizer.
 *
 *  === Token
 *
 *  A Token is a single term from a document field. A token contains the text
 *  representing the term as well as the start and end offset of the token.
 *  The start and end offset will represent the token as it appears in the
 *  source field. Some TokenFilters may change the text in the Token but the
 *  start and end offsets should stay the same so (end - start) won't
 *  necessarily be equal to the length of text in the token. For example using
 *  a stemming TokenFilter the term "Beginning" might have start and end
 *  offsets of 10 and 19 respectively ("Beginning".length == 9) but Token#text
 *  might be "begin" (after stemming).
 */
void Init_Analysis(void) {
    mAnalysis = rb_define_module_under(mFerret, "Analysis");

    /* TokenStream Methods */
    id_next = rb_intern("next");
    id_reset = rb_intern("text=");
    id_clone = rb_intern("clone");
    id_text = rb_intern("@text");

    /* FrtAnalyzer Methods */
    id_token_stream = rb_intern("token_stream");

    object_space = rb_hash_new();
    rb_define_const(mFerret, "OBJECT_SPACE", object_space);

    rb_define_const(mAnalysis, "ENGLISH_STOP_WORDS", get_rstopwords(FRT_ENGLISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_ENGLISH_STOP_WORDS", get_rstopwords(FRT_FULL_ENGLISH_STOP_WORDS));
    rb_define_const(mAnalysis, "EXTENDED_ENGLISH_STOP_WORDS", get_rstopwords(FRT_EXTENDED_ENGLISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_FRENCH_STOP_WORDS", get_rstopwords(FRT_FULL_FRENCH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_SPANISH_STOP_WORDS", get_rstopwords(FRT_FULL_SPANISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_PORTUGUESE_STOP_WORDS", get_rstopwords(FRT_FULL_PORTUGUESE_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_ITALIAN_STOP_WORDS", get_rstopwords(FRT_FULL_ITALIAN_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_GERMAN_STOP_WORDS", get_rstopwords(FRT_FULL_GERMAN_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_DUTCH_STOP_WORDS", get_rstopwords(FRT_FULL_DUTCH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_SWEDISH_STOP_WORDS", get_rstopwords(FRT_FULL_SWEDISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_NORWEGIAN_STOP_WORDS", get_rstopwords(FRT_FULL_NORWEGIAN_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_DANISH_STOP_WORDS", get_rstopwords(FRT_FULL_DANISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_RUSSIAN_STOP_WORDS", get_rstopwords(FRT_FULL_RUSSIAN_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_FINNISH_STOP_WORDS", get_rstopwords(FRT_FULL_FINNISH_STOP_WORDS));
    rb_define_const(mAnalysis, "FULL_HUNGARIAN_STOP_WORDS", get_rstopwords(FRT_FULL_HUNGARIAN_STOP_WORDS));

    Init_Token();
    Init_TokenStream();

    Init_LetterTokenizer();
    Init_WhiteSpaceTokenizer();
    Init_StandardTokenizer();
    Init_RegExpTokenizer();

    Init_LowerCaseFilter();
    Init_HyphenFilter();
    Init_StopFilter();
    Init_MappingFilter();
    Init_StemFilter();

    Init_Analyzer();
    Init_LetterAnalyzer();
    Init_WhiteSpaceAnalyzer();
    Init_StandardAnalyzer();
    Init_PerFieldAnalyzer();
    Init_RegExpAnalyzer();
}
