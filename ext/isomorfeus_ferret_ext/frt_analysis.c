#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include "frt_analysis.h"
#include "frt_hash.h"
#include "libstemmer.h"
#include "frt_scanner.h"

/* initialized in frt_global.c */
extern rb_encoding *utf8_encoding;
extern OnigCodePoint cp_apostrophe;
extern OnigCodePoint cp_dot;
extern OnigCodePoint cp_comma;
extern OnigCodePoint cp_backslash;
extern OnigCodePoint cp_slash;
extern OnigCodePoint cp_underscore;
extern OnigCodePoint cp_dash;

/****************************************************************************
 *
 * Token
 *
 ****************************************************************************/

FrtToken *frt_tk_set(FrtToken *tk, char *text, int tlen, off_t start, off_t end, int pos_inc, rb_encoding *encoding)
{
    if (tlen >= FRT_MAX_WORD_SIZE) {
        tlen = FRT_MAX_WORD_SIZE - 1;
    }
    if (encoding == utf8_encoding) {
        memcpy(tk->text, text, sizeof(char) * tlen);
    } else {
        const unsigned char *sp = (unsigned char *)text;
        unsigned char *dp = (unsigned char *)tk->text;
        rb_econv_t *ec = rb_econv_open(rb_enc_name(encoding), rb_enc_name(utf8_encoding), RUBY_ECONV_INVALID_REPLACE);
        assert(ec != NULL);
        rb_econv_convert(ec, &sp, (unsigned char *)text + tlen, &dp, (unsigned char *)tk->text + FRT_MAX_WORD_SIZE - 1, 0);
        rb_econv_close(ec);
        tlen = dp - (unsigned char *)tk->text;
    }
    tk->text[tlen] = '\0';
    tk->len = tlen;    // in utf8_encoding
    tk->start = start; // in original encoding
    tk->end = end;     // in original encoding
    tk->pos_inc = pos_inc;
    return tk;
}

static FrtToken *frt_tk_set_ts(FrtToken *tk, char *start, char *end, char *text, int pos_inc, rb_encoding *encoding)
{
    return frt_tk_set(tk, start, (int)(end - start), (off_t)(start - text), (off_t)(end - text), pos_inc, encoding);
}

FrtToken *frt_tk_set_no_len(FrtToken *tk, char *text, off_t start, off_t end, int pos_inc, rb_encoding *encoding)
{
    return frt_tk_set(tk, text, (int)strlen(text), start, end, pos_inc, encoding);
}

int frt_tk_eq(FrtToken *tk1, FrtToken *tk2)
{
    return (strcmp((char *)tk1->text, (char *)tk2->text) == 0 &&
            tk1->start == tk2->start && tk1->end == tk2->end &&
            tk1->pos_inc == tk2->pos_inc);
}

int frt_tk_cmp(FrtToken *tk1, FrtToken *tk2)
{
    int cmp;
    if (tk1->start > tk2->start) {
        cmp = 1;
    }
    else if (tk1->start < tk2->start) {
        cmp = -1;
    }
    else {
        if (tk1->end > tk2->end) {
            cmp = 1;
        }
        else if (tk1->end < tk2->end) {
            cmp = -1;
        }
        else {
            cmp = strcmp((char *)tk1->text, (char *)tk2->text);
        }
    }
    return cmp;
}

void frt_tk_destroy(void *p)
{
    free(p);
}

FrtToken *frt_tk_new()
{
    return FRT_ALLOC(FrtToken);
}
/****************************************************************************
 *
 * TokenStream
 *
 ****************************************************************************/

void frt_ts_deref(FrtTokenStream *ts)
{
    if (--ts->ref_cnt <= 0) {
        ts->destroy_i(ts);
    }
}

static FrtTokenStream *ts_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding)
{
    ts->t = ts->text = text;
    ts->length = strlen(text);
    ts->encoding = encoding;
    return ts;
}

FrtTokenStream *frt_ts_clone_size(FrtTokenStream *orig_ts, size_t size)
{
    FrtTokenStream *ts = (FrtTokenStream *)frt_ecalloc(size);
    memcpy(ts, orig_ts, size);
    ts->ref_cnt = 1;
    return ts;
}

FrtTokenStream *frt_ts_new_i(size_t size)
{
    FrtTokenStream *ts = (FrtTokenStream *)frt_ecalloc(size);

    ts->destroy_i = (void (*)(FrtTokenStream *))&free;
    ts->reset = &ts_reset;
    ts->ref_cnt = 1;

    return ts;
}

/****************************************************************************
 * CachedTokenStream
 ****************************************************************************/

#define CTS(token_stream) ((FrtCachedTokenStream *)(token_stream))

static FrtTokenStream *cts_clone_i(FrtTokenStream *orig_ts)
{
    return frt_ts_clone_size(orig_ts, sizeof(FrtCachedTokenStream));
}

static FrtTokenStream *cts_new()
{
    FrtTokenStream *ts = frt_ts_new(FrtCachedTokenStream);
    ts->clone_i = &cts_clone_i;
    return ts;
}

/* * Multi-byte TokenStream * */

#define MBTS(token_stream) ((FrtMultiByteTokenStream *)(token_stream))

static FrtTokenStream *mb_ts_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding)
{
    FRT_ZEROSET(&(MBTS(ts)->state), mbstate_t);
    ts_reset(ts, text, encoding);
    return ts;
}

static FrtTokenStream *mb_ts_clone_i(FrtTokenStream *orig_ts)
{
    return frt_ts_clone_size(orig_ts, sizeof(FrtMultiByteTokenStream));
}

static FrtTokenStream *mb_ts_new()
{
    FrtTokenStream *ts = frt_ts_new(FrtMultiByteTokenStream);
    ts->reset = &mb_ts_reset;
    ts->clone_i = &mb_ts_clone_i;
    ts->ref_cnt = 1;
    return ts;
}

/****************************************************************************
 *
 * Analyzer
 *
 ****************************************************************************/

void frt_a_deref(FrtAnalyzer *a)
{
    if (--a->ref_cnt <= 0) {
        a->destroy_i(a);
    }
}

static void frt_a_standard_destroy_i(FrtAnalyzer *a)
{
    if (a->current_ts) {
        frt_ts_deref(a->current_ts);
    }
    free(a);
}

static FrtTokenStream *a_standard_get_ts(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding)
{
    FrtTokenStream *ts;
    (void)field;
    ts = frt_ts_clone(a->current_ts);
    return ts->reset(ts, text, encoding);
}

FrtAnalyzer *frt_analyzer_new(FrtTokenStream *ts,
                       void (*destroy_i)(FrtAnalyzer *a),
                       FrtTokenStream *(*get_ts)(FrtAnalyzer *a, FrtSymbol field, char *text, rb_encoding *encoding))
{
    FrtAnalyzer *a = FRT_ALLOC(FrtAnalyzer);
    a->current_ts = ts;
    a->destroy_i = (destroy_i ? destroy_i : &frt_a_standard_destroy_i);
    a->get_ts = (get_ts ? get_ts : &a_standard_get_ts);
    a->ref_cnt = 1;
    return a;
}

/****************************************************************************
 *
 * Non
 *
 ****************************************************************************/

/*
 * NonTokenizer
 */
static FrtToken *nt_next(FrtTokenStream *ts)
{
    if (ts->t) {
        size_t len = strlen(ts->t);
        ts->t = NULL;

        return frt_tk_set(&(CTS(ts)->token), ts->text, len, 0, len, 1, ts->encoding);
    }
    else {
        return NULL;
    }
}

FrtTokenStream *frt_non_tokenizer_new()
{
    FrtTokenStream *ts = cts_new();
    ts->next = &nt_next;
    return ts;
}

/*
 * NonAnalyzer
 */
FrtAnalyzer *frt_non_analyzer_new()
{
    return frt_analyzer_new(frt_non_tokenizer_new(), NULL, NULL);
}

/****************************************************************************
 *
 * Whitespace
 *
 ****************************************************************************/

/*
 * Multi-byte WhitespaceTokenizer
 */
static FrtToken *mb_wst_next(FrtTokenStream *ts)
{
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *start;
    char *t = ts->t;

    if (t == end) { return NULL; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    while (cp_len > 0 && rb_enc_isspace(cp, enc)) {
        t += cp_len;
        if (t == end) { return NULL; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }

    start = t;

    do {
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    } while (cp_len > 0 && !rb_enc_isspace(cp, enc));
    ts->t = t;
    return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
}

FrtTokenStream *frt_mb_whitespace_tokenizer_new(bool lowercase)
{
    FrtTokenStream *ts = mb_ts_new();
    ts->next = &mb_wst_next;
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    return ts;
}

/*
 * WhitespaceAnalyzer
 */

FrtAnalyzer *frt_mb_whitespace_analyzer_new(bool lowercase)
{
    return frt_analyzer_new(frt_mb_whitespace_tokenizer_new(lowercase), NULL, NULL);
}

/****************************************************************************
 *
 * Letter
 *
 ****************************************************************************/

/*
 * Multi-byte LetterTokenizer
 */
static FrtToken *mb_lt_next(FrtTokenStream *ts)
{
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *start;
    char *t = ts->t;

    if (t == end) { return NULL; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    while (cp_len > 0 && !rb_enc_isalpha(cp, enc)) {
        t += cp_len;
        if (t == end) { return NULL; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }

    start = t;

    do {
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    } while (cp_len > 0 && rb_enc_isalpha(cp, enc));
    ts->t = t;
    return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
}

/*
 * Lowercasing Multi-byte LetterTokenizer
 */
static FrtToken *mb_lt_next_lc(FrtTokenStream *ts)
{
    int cp_len = 0;
    OnigCaseFoldType fold_type = ONIGENC_CASE_DOWNCASE;
    OnigCodePoint cp, cpl;
    rb_encoding *enc = ts->encoding;
    char buf[FRT_MAX_WORD_SIZE + 1];
    int len = 0;
    char *buf_end = buf + FRT_MAX_WORD_SIZE - rb_enc_mbmaxlen(enc); // space for longest possible mulibyte at the end
    char *end = ts->text + ts->length;
    char *start, *begin;
    char *t = ts->t;

    if (t == end) { return NULL; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    while (cp_len > 0 && !rb_enc_isalpha(cp, enc)) {
        t += cp_len;
        if (t == end) { return NULL; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }

    begin = start = t;

    do {
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    } while (cp_len > 0 && rb_enc_isalpha(cp, enc));

    len = enc->case_map(&fold_type, &begin, t, buf, buf_end, enc);
    *(buf + len) = '\0';
    ts->t = t;
    return frt_tk_set(&(CTS(ts)->token), buf, len, (off_t)(start - ts->text), (off_t)(t - ts->text), 1, enc);
}

FrtTokenStream *frt_mb_letter_tokenizer_new(bool lowercase)
{
    FrtTokenStream *ts = mb_ts_new();
    ts->next = lowercase ? &mb_lt_next_lc : &mb_lt_next;
    return ts;
}

FrtAnalyzer *frt_mb_letter_analyzer_new(bool lowercase)
{
    return frt_analyzer_new(frt_mb_letter_tokenizer_new(lowercase), NULL, NULL);
}

/****************************************************************************
 *
 * Standard
 *
 ****************************************************************************/

#define STDTS(token_stream) ((FrtStandardTokenizer *)(token_stream))

/*
 * FrtStandardTokenizer
 */
static FrtToken *std_next(FrtTokenStream *ts)
{
    rb_encoding *enc = ts->encoding;
    FrtStandardTokenizer *std_tz = STDTS(ts);
    const char *start = NULL;
    const char *end = NULL;
    int len;
    FrtToken *tk = &(CTS(ts)->token);

    if (enc == utf8_encoding) {
        frt_std_scan_utf8(ts->t, tk->text, FRT_MAX_WORD_SIZE - 1, &start, &end, &len);
    } else {
        rb_raise(rb_eNotImpError, "TokenStream data must be in utf8 encoding");
    }

    if (len == 0)
        return NULL;

    ts->t       = (char *)end;
    *(tk->text + len) = '\0';
    tk->len     = len;
    tk->start   = start - ts->text;
    tk->end     = end   - ts->text;
    tk->pos_inc = 1;
    return &(CTS(ts)->token);
}

static FrtTokenStream *std_ts_clone_i(FrtTokenStream *orig_ts)
{
    return frt_ts_clone_size(orig_ts, sizeof(FrtStandardTokenizer));
}

static FrtTokenStream *std_ts_new()
{
    FrtTokenStream *ts = frt_ts_new(FrtStandardTokenizer);
    ts->clone_i        = &std_ts_clone_i;
    ts->next           = &std_next;

    return ts;
}

FrtTokenStream *frt_mb_standard_tokenizer_new()
{
    FrtTokenStream *ts = std_ts_new();
    return ts;
}

/****************************************************************************
 *
 * LegacyStandard
 *
 ****************************************************************************/

#define LSTDTS(token_stream) ((FrtLegacyStandardTokenizer *)(token_stream))

/*
 * LegacyStandardTokenizer
 */

static int mb_legacy_std_get_alpha(FrtTokenStream *ts, char *token)
{
    int i;
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *t = ts->t;

    if (t == end) { return 0; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    while (cp_len > 0 && rb_enc_isalnum(cp, enc)) {
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }

    i = (int)(t - ts->t);
    if (i > FRT_MAX_WORD_SIZE) {
        i = FRT_MAX_WORD_SIZE - 1;
    }
    memcpy(token, ts->t, i);
    return i;
}

static int cp_isnumpunc(OnigCodePoint cp) {
    return (cp == cp_dot || cp == cp_comma || cp == cp_backslash
            || cp == cp_slash || cp == cp_underscore || cp == cp_dash);
}

static int isnumpunc(char c)
{
    return (c == '.' || c == ',' || c == '\\' || c == '/' || c == '_'
            || c == '-');
}

static int w_isnumpunc(wchar_t c)
{
    return (c == L'.' || c == L',' || c == L'\\' || c == L'/' || c == L'_'
            || c == L'-');
}

static int isurlpunc(char c)
{
    return (c == '.' || c == '/' || c == '-' || c == '_');
}

static int isurlc(char c)
{
    return (c == '.' || c == '/' || c == '-' || c == '_' || isalnum(c));
}

static int isurlxatpunc(char c)
{
    return (c == '.' || c == '/' || c == '-' || c == '_' || c == '@');
}

static int isurlxatc(char c)
{
    return (c == '.' || c == '/' || c == '-' || c == '_' || c == '@'
            || isalnum(c));
}

static bool mb_legacy_std_is_tok_char(char *t)
{
    wchar_t c;
    mbstate_t state; FRT_ZEROSET(&state, mbstate_t);

    if (((int)mbrtowc(&c, t, MB_CUR_MAX, &state)) < 0) {
        /* error which we can handle next time round. For now just return
         * false so that we can return a token */
        return false;
    }
    if (iswspace(c)) {
        return false;           /* most common so check first. */
    }
    if (iswalnum(c) || w_isnumpunc(c) || c == L'&' || c == L'@' || c == L'\''
        || c == L':') {
        return true;
    }
    return false;
}

/* (alnum)((punc)(alnum))+ where every second sequence of alnum must contain at
 * least one digit.
 * (alnum) = [a-zA-Z0-9]
 * (punc) = [_\/.,-]
 */
static int legacy_std_get_number(char *input)
{
    int i = 0;
    int count = 0;
    int last_seen_digit = 2;
    int seen_digit = false;

    while (last_seen_digit >= 0) {
        while ((input[i] != '\0') && isalnum(input[i])) {
            if ((last_seen_digit < 2) && isdigit(input[i])) {
                last_seen_digit = 2;
            }
            if ((seen_digit == false) && isdigit(input[i])) {
                seen_digit = true;
            }
            i++;
        }
        last_seen_digit--;
        if (!isnumpunc(input[i]) || !isalnum(input[i + 1])) {

            if (last_seen_digit >= 0) {
                count = i;
            }
            break;
        }
        count = i;
        i++;
    }
    if (seen_digit) {
        return count;
    }
    else {
        return 0;
    }
}

static int mb_legacy_std_get_apostrophe(FrtTokenStream *ts, char *input)
{
    int cp_len = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *t = input;

    if (t == end) { return 0; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);

    while (rb_enc_isalpha(cp, enc) || cp == cp_apostrophe) {
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }
    return (int)(t - input);
}

static char *std_get_url(char *input, char *token, int i, int *len)
{
    char *next = NULL;
    while (isurlc(input[i])) {
        if (isurlpunc(input[i]) && isurlpunc(input[i - 1])) {
            break; /* can't have two puncs in a row */
        }
        if (i < FRT_MAX_WORD_SIZE) {
            token[i] = input[i];
        }
        i++;
    }
    next = input + i;

    /* We don't want to index past the end of the token capacity) */
    if (i >= FRT_MAX_WORD_SIZE) {
        i = FRT_MAX_WORD_SIZE - 1;
    }

    /* strip trailing puncs */
    while (isurlpunc(input[i - 1])) {
        i--;
    }
    *len = i;
    token[i] = '\0';

    return next;
}

/* Company names can contain '@' and '&' like AT&T and Excite@Home. Let's
*/
static int legacy_std_get_company_name(char *input)
{
    int i = 0;
    while (isalpha(input[i]) || input[i] == '@' || input[i] == '&') {
        i++;
    }

    return i;
}

static bool mb_legacy_std_advance_to_start(FrtTokenStream *ts)
{
    int cp_len = 0;
    int cp_len_b = 0;
    OnigCodePoint cp;
    rb_encoding *enc = ts->encoding;
    char *end = ts->text + ts->length;
    char *t = ts->t;

    if (t == end) { return false; }
    cp = rb_enc_codepoint_len(t, end, &cp_len, enc);

    while (cp_len > 0 && !rb_enc_isalnum(cp, enc)) {
        if (cp_isnumpunc(cp)) {
            cp = rb_enc_codepoint_len(t + cp_len, end, &cp_len_b, enc);
            if (rb_enc_isdigit(cp, enc)) break;
        }
        t += cp_len;
        if (t == end) { break; }
        cp = rb_enc_codepoint_len(t, end, &cp_len, enc);
    }
    ts->t = t;
    return (t < end);
}

static FrtToken *legacy_std_next(FrtTokenStream *ts)
{
    FrtLegacyStandardTokenizer *std_tz = LSTDTS(ts);
    char *s;
    char *t;
    char *start = NULL;
    char *num_end = NULL;
    char token[FRT_MAX_WORD_SIZE + 1];
    int token_i = 0;
    int len;
    bool is_acronym;
    bool seen_at_symbol;
    rb_encoding *enc = ts->encoding;

    if (!std_tz->advance_to_start(ts)) {
        return NULL;
    }

    start = t = ts->t;
    token_i = std_tz->get_alpha(ts, token);
    t += token_i;

    if (!std_tz->is_tok_char(t)) {
        /* very common case, ie a plain word, so check and return */
        ts->t = t;
        return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
    }

    if (*t == '\'') {       /* apostrophe case. */
        t += std_tz->get_apostrophe(ts, t);
        ts->t = t;
        len = (int)(t - start);
        /* strip possesive */
        if ((t[-1] == 's' || t[-1] == 'S') && t[-2] == '\'') {
            t -= 2;
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
            CTS(ts)->token.end += 2;
        }
        else if (t[-1] == '\'') {
            t -= 1;
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
            CTS(ts)->token.end += 1;
        }
        else {
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
        }

        return &(CTS(ts)->token);
    }

    if (*t == '&') {        /* apostrophe case. */
        t += legacy_std_get_company_name(t);
        ts->t = t;
        return frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
    }

    if ((isdigit(*start) || isnumpunc(*start))       /* possibly a number */
        && ((len = legacy_std_get_number(start)) > 0)) {
        num_end = start + len;
        if (!std_tz->is_tok_char(num_end)) { /* won't find a longer token */
            ts->t = num_end;
            return frt_tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1, enc);
        }
        /* else there may be a longer token so check */
    }

    if (t[0] == ':' && t[1] == '/' && t[2] == '/') {
        /* check for a known url start */
        token[token_i] = '\0';
        t += 3;
        token_i += 3;
        while (*t == '/') {
            t++;
        }
        if (isalpha(*t) &&
            (memcmp(token, "ftp", 3) == 0 ||
             memcmp(token, "http", 4) == 0 ||
             memcmp(token, "https", 5) == 0 ||
             memcmp(token, "file", 4) == 0)) {
            ts->t = std_get_url(t, token, 0, &len); /* dispose of first part of the URL */
        }
        else {              /* still treat as url but keep the first part */
            token_i = (int)(t - start);
            memcpy(token, start, token_i * sizeof(char));
            ts->t = std_get_url(start, token, token_i, &len); /* keep start */
        }
        return frt_tk_set(&(CTS(ts)->token), token, len,
                      (off_t)(start - ts->text),
                      (off_t)(ts->t - ts->text), 1, enc);
    }

    /* now see how long a url we can find. */
    is_acronym = true;
    seen_at_symbol = false;
    while (isurlxatc(*t)) {
        if (is_acronym && !isalpha(*t) && (*t != '.')) {
            is_acronym = false;
        }
        if (isurlxatpunc(*t) && isurlxatpunc(t[-1])) {
            break; /* can't have two punctuation characters in a row */
        }
        if (*t == '@') {
            if (seen_at_symbol) {
                break; /* we can only have one @ symbol */
            }
            else {
                seen_at_symbol = true;
            }
        }
        t++;
    }
    while (isurlxatpunc(t[-1]) && t > ts->t) {
        t--;                /* strip trailing punctuation */
    }

    if (t < ts->t || (num_end != NULL && num_end < ts->t)) {
        fprintf(stderr, "Warning: encoding error. Please check that you are using the correct locale for your input");
        return NULL;
    } else if (num_end == NULL || t > num_end) {
        ts->t = t;

        if (is_acronym) {   /* check it is one letter followed by one '.' */
            for (s = start; s < t - 1; s++) {
                if (isalpha(*s) && (s[1] != '.'))
                    is_acronym = false;
            }
        }
        if (is_acronym) {   /* strip '.'s */
            for (s = start + token_i; s < t; s++) {
                if (*s != '.') {
                    token[token_i] = *s;
                    token_i++;
                }
            }
            frt_tk_set(&(CTS(ts)->token), token, token_i,
                   (off_t)(start - ts->text),
                   (off_t)(t - ts->text), 1, enc);
        }
        else { /* just return the url as is */
            frt_tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1, enc);
        }
    }
    else {                  /* return the number */
        ts->t = num_end;
        frt_tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1, enc);
    }

    return &(CTS(ts)->token);
}

static FrtTokenStream *legacy_std_ts_clone_i(FrtTokenStream *orig_ts)
{
    return frt_ts_clone_size(orig_ts, sizeof(FrtLegacyStandardTokenizer));
}

static FrtTokenStream *legacy_std_ts_new()
{
    FrtTokenStream *ts = frt_ts_new(FrtLegacyStandardTokenizer);
    ts->clone_i        = &legacy_std_ts_clone_i;
    ts->next           = &legacy_std_next;

    return ts;
}

FrtTokenStream *frt_mb_legacy_standard_tokenizer_new()
{
    FrtTokenStream *ts = legacy_std_ts_new();

    LSTDTS(ts)->advance_to_start = &mb_legacy_std_advance_to_start;
    LSTDTS(ts)->get_alpha        = &mb_legacy_std_get_alpha;
    LSTDTS(ts)->is_tok_char      = &mb_legacy_std_is_tok_char;
    LSTDTS(ts)->get_apostrophe   = &mb_legacy_std_get_apostrophe;

    return ts;
}

/****************************************************************************
 *
 * Filters
 *
 ****************************************************************************/

#define TkFilt(filter) ((FrtTokenFilter *)(filter))

FrtTokenStream *frt_filter_clone_size(FrtTokenStream *ts, size_t size)
{
    FrtTokenStream *ts_new = frt_ts_clone_size(ts, size);
    TkFilt(ts_new)->sub_ts = TkFilt(ts)->sub_ts->clone_i(TkFilt(ts)->sub_ts);
    return ts_new;
}

static FrtTokenStream *filter_clone_i(FrtTokenStream *ts)
{
    return frt_filter_clone_size(ts, sizeof(FrtTokenFilter));
}

static FrtTokenStream *filter_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding)
{
    TkFilt(ts)->sub_ts->reset(TkFilt(ts)->sub_ts, text, encoding);
    return ts;
}

static void filter_destroy_i(FrtTokenStream *ts)
{
    frt_ts_deref(TkFilt(ts)->sub_ts);
    free(ts);
}

FrtTokenStream *frt_tf_new_i(size_t size, FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts     = (FrtTokenStream *)frt_ecalloc(size);

    TkFilt(ts)->sub_ts  = sub_ts;

    ts->clone_i         = &filter_clone_i;
    ts->destroy_i       = &filter_destroy_i;
    ts->reset           = &filter_reset;
    ts->ref_cnt         = 1;

    return ts;
}

/****************************************************************************
 * FrtStopFilter
 ****************************************************************************/

#define StopFilt(filter) ((FrtStopFilter *)(filter))

static void sf_destroy_i(FrtTokenStream *ts)
{
    frt_h_destroy(StopFilt(ts)->words);
    filter_destroy_i(ts);
}

static FrtTokenStream *sf_clone_i(FrtTokenStream *orig_ts)
{
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
    FRT_REF(StopFilt(new_ts)->words);
    return new_ts;
}

static FrtToken *sf_next(FrtTokenStream *ts)
{
    int pos_inc = 0;
    FrtHash *words = StopFilt(ts)->words;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);

    while ((tk != NULL) && (frt_h_get(words, tk->text) != NULL)) {
        pos_inc += tk->pos_inc;
        tk = tf->sub_ts->next(tf->sub_ts);
    }

    if (tk != NULL) {
        tk->pos_inc += pos_inc;
    }

    return tk;
}

FrtTokenStream *frt_stop_filter_new_with_words_len(FrtTokenStream *sub_ts,
                                            const char **words, int len)
{
    int i;
    char *word;
    FrtHash *word_table = frt_h_new_str(&free, (frt_free_ft) NULL);
    FrtTokenStream *ts = tf_new(FrtStopFilter, sub_ts);

    for (i = 0; i < len; i++) {
        word = frt_estrdup(words[i]);
        frt_h_set(word_table, word, word);
    }
    StopFilt(ts)->words = word_table;
    ts->next            = &sf_next;
    ts->destroy_i       = &sf_destroy_i;
    ts->clone_i         = &sf_clone_i;
    return ts;
}

FrtTokenStream *frt_stop_filter_new_with_words(FrtTokenStream *sub_ts,
                                        const char **words)
{
    char *word;
    FrtHash *word_table = frt_h_new_str(&free, (frt_free_ft) NULL);
    FrtTokenStream *ts = tf_new(FrtStopFilter, sub_ts);

    while (*words) {
        word = frt_estrdup(*words);
        frt_h_set(word_table, word, word);
        words++;
    }

    StopFilt(ts)->words = word_table;
    ts->next            = &sf_next;
    ts->destroy_i       = &sf_destroy_i;
    ts->clone_i         = &sf_clone_i;
    return ts;
}

FrtTokenStream *frt_stop_filter_new(FrtTokenStream *ts)
{
    return frt_stop_filter_new_with_words(ts, FRT_FULL_ENGLISH_STOP_WORDS);
}

/****************************************************************************
 * MappingFilter
 ****************************************************************************/

#define MFilt(filter) ((FrtMappingFilter *)(filter))

static void mf_destroy_i(FrtTokenStream *ts)
{
    frt_mulmap_destroy(MFilt(ts)->mapper);
    filter_destroy_i(ts);
}

static FrtTokenStream *mf_clone_i(FrtTokenStream *orig_ts)
{
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
    FRT_REF(MFilt(new_ts)->mapper);
    return new_ts;
}

static FrtToken *mf_next(FrtTokenStream *ts)
{
    char buf[FRT_MAX_WORD_SIZE + 1];
    FrtMultiMapper *mapper = MFilt(ts)->mapper;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);
    if (tk != NULL) {
        tk->len = frt_mulmap_map_len(mapper, buf, tk->text, FRT_MAX_WORD_SIZE);
        memcpy(tk->text, buf, tk->len + 1);
    }
    return tk;
}

static FrtTokenStream *mf_reset(FrtTokenStream *ts, char *text, rb_encoding *encoding)
{
    FrtMultiMapper *mm = MFilt(ts)->mapper;
    if (mm->d_size == 0) {
        frt_mulmap_compile(MFilt(ts)->mapper);
    }
    filter_reset(ts, text, encoding);
    return ts;
}

FrtTokenStream *frt_mapping_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtMappingFilter, sub_ts);
    MFilt(ts)->mapper  = frt_mulmap_new();
    ts->next           = &mf_next;
    ts->destroy_i      = &mf_destroy_i;
    ts->clone_i        = &mf_clone_i;
    ts->reset          = &mf_reset;
    return ts;
}

FrtTokenStream *frt_mapping_filter_add(FrtTokenStream *ts, const char *pattern,
                                const char *replacement)
{
    frt_mulmap_add_mapping(MFilt(ts)->mapper, pattern, replacement);
    return ts;
}

/****************************************************************************
 * HyphenFilter
 ****************************************************************************/

#define HyphenFilt(filter) ((FrtHyphenFilter *)(filter))

static FrtTokenStream *hf_clone_i(FrtTokenStream *orig_ts)
{
    FrtTokenStream *new_ts = frt_filter_clone_size(orig_ts, sizeof(FrtHyphenFilter));
    return new_ts;
}

static FrtToken *hf_next(FrtTokenStream *ts)
{
    FrtHyphenFilter *hf = HyphenFilt(ts);
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = hf->tk;

    if (hf->pos < hf->len) {
        const int pos = hf->pos;
        const int text_len = strlen(hf->text + pos);
        strcpy(tk->text, hf->text + pos);
        tk->pos_inc = ((pos != 0) ? 1 : 0);
        tk->start = hf->start + pos;
        tk->end = tk->start + text_len;
        hf->pos += text_len + 1;
        tk->len = text_len;
        return tk;
    }
    else {
        char *p;
        bool seen_hyphen = false;
        bool seen_other_punc = false;
        hf->tk = tk = tf->sub_ts->next(tf->sub_ts);
        if (NULL == tk) return NULL;
        p = tk->text + 1;
        while (*p) {
            if (*p == '-') {
                seen_hyphen = true;
            }
            else if (!isalpha(*p)) {
                seen_other_punc = true;
                break;
            }
            p++;
        }
        if (seen_hyphen && !seen_other_punc) {
            char *q = hf->text;
            char *r = tk->text;
            p = tk->text;
            while (*p) {
                if (*p == '-') {
                    *q = '\0';
                }
                else {
                    *r = *q = *p;
                    r++;
                }
                q++;
                p++;
            }
            *r = *q = '\0';
            hf->start = tk->start;
            hf->pos = 0;
            hf->len = q - hf->text;
            tk->len = r - tk->text;
        }
    }
    return tk;
}

FrtTokenStream *frt_hyphen_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtHyphenFilter, sub_ts);
    ts->next           = &hf_next;
    ts->clone_i        = &hf_clone_i;
    return ts;
}

/****************************************************************************
 * LowerCaseFilter
 ****************************************************************************/


static FrtToken *mb_lcf_next(FrtTokenStream *ts)
{
    int len = 0;
    OnigCaseFoldType fold_type = ONIGENC_CASE_DOWNCASE;
    rb_encoding *enc = utf8_encoding; // Token encoding is always UTF-8
    char buf[FRT_MAX_WORD_SIZE + 20]; // CASE_MAPPING_ADDITIONAL_LENGTH
    char *buf_end = buf + FRT_MAX_WORD_SIZE + 19;

    FrtToken *tk = TkFilt(ts)->sub_ts->next(TkFilt(ts)->sub_ts);
    if (tk == NULL) { return tk; }

    char *t = tk->text;

    len = enc->case_map(&fold_type, &t, tk->text + tk->len, buf, buf_end, enc);
    tk->len = len;
    memcpy(tk->text, buf, len);
    tk->text[len] = '\0';

    return tk;
}

FrtTokenStream *frt_mb_lowercase_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtTokenFilter, sub_ts);
    ts->next = &mb_lcf_next;
    return ts;
}

/****************************************************************************
 * FrtStemFilter
 ****************************************************************************/

#define StemFilt(filter) ((FrtStemFilter *)(filter))

static void stemf_destroy_i(FrtTokenStream *ts)
{
    sb_stemmer_delete(StemFilt(ts)->stemmer);
    free(StemFilt(ts)->algorithm);
    free(StemFilt(ts)->charenc);
    filter_destroy_i(ts);
}

static FrtToken *stemf_next(FrtTokenStream *ts)
{
    int len;
    const sb_symbol *stemmed;
    struct sb_stemmer *stemmer = StemFilt(ts)->stemmer;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);
    if (tk == NULL) {
        return tk;
    }
    stemmed = sb_stemmer_stem(stemmer, (sb_symbol *)tk->text, tk->len);
    len = sb_stemmer_length(stemmer);
    if (len >= FRT_MAX_WORD_SIZE) {
        len = FRT_MAX_WORD_SIZE - 1;
    }

    memcpy(tk->text, stemmed, len);
    tk->text[len] = '\0';
    tk->len = len;
    return tk;
}

static FrtTokenStream *stemf_clone_i(FrtTokenStream *orig_ts)
{
    FrtTokenStream *new_ts      = frt_filter_clone_size(orig_ts, sizeof(FrtStemFilter));
    FrtStemFilter *stemf        = StemFilt(new_ts);
    FrtStemFilter *orig_stemf   = StemFilt(orig_ts);
    stemf->stemmer =
        sb_stemmer_new(orig_stemf->algorithm, orig_stemf->charenc);
    stemf->algorithm =
        orig_stemf->algorithm ? frt_estrdup(orig_stemf->algorithm) : NULL;
    stemf->charenc =
        orig_stemf->charenc ? frt_estrdup(orig_stemf->charenc) : NULL;
    return new_ts;
}

FrtTokenStream *frt_stem_filter_new(FrtTokenStream *ts, const char *algorithm,
                             const char *charenc)
{
    FrtTokenStream *tf = tf_new(FrtStemFilter, ts);
    char *my_algorithm = NULL;
    char *my_charenc   = NULL;
    char *s = NULL;

    if (algorithm) {
        my_algorithm = frt_estrdup(algorithm);

        /* algorithms are lowercase */
        s = my_algorithm;
        while (*s) {
            *s = tolower(*s);
            s++;
        }
        StemFilt(tf)->algorithm = my_algorithm;
    }

    if (charenc) {
        my_charenc   = frt_estrdup(charenc);

        /* encodings are uppercase and use '_' instead of '-' */
        s = my_charenc;
        while (*s) {
            *s = (*s == '-') ? '_' : toupper(*s);
            s++;
        }
        StemFilt(tf)->charenc = my_charenc;
    }

    StemFilt(tf)->stemmer   = sb_stemmer_new(my_algorithm, my_charenc);

    tf->next = &stemf_next;
    tf->destroy_i = &stemf_destroy_i;
    tf->clone_i = &stemf_clone_i;
    return tf;
}

/****************************************************************************
 *
 * Analyzers
 *
 ****************************************************************************/

/****************************************************************************
 * Standard
 ****************************************************************************/

FrtAnalyzer *frt_mb_standard_analyzer_new_with_words(const char **words,
                                              bool lowercase)
{
    FrtTokenStream *ts = frt_mb_standard_tokenizer_new();
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    ts = frt_hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_mb_standard_analyzer_new(bool lowercase)
{
    return frt_mb_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                               lowercase);
}

/****************************************************************************
 * Legacy
 ****************************************************************************/

FrtAnalyzer *frt_mb_legacy_standard_analyzer_new_with_words(const char **words,
                                                     bool lowercase)
{
    FrtTokenStream *ts = frt_mb_legacy_standard_tokenizer_new();
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    ts = frt_hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_mb_legacy_standard_analyzer_new(bool lowercase)
{
    return frt_mb_legacy_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                                      lowercase);
}

/****************************************************************************
 *
 * PerFieldAnalyzer
 *
 ****************************************************************************/

static void pfa_destroy_i(FrtAnalyzer *self)
{
    frt_h_destroy(PFA(self)->dict);

    frt_a_deref(PFA(self)->default_a);
    free(self);
}

static FrtTokenStream *pfa_get_ts(FrtAnalyzer *self, FrtSymbol field, char *text, rb_encoding *encoding)
{
    FrtAnalyzer *a = (FrtAnalyzer *)frt_h_get(PFA(self)->dict, (void *)field);
    if (a == NULL) {
        a = PFA(self)->default_a;
    }
    return frt_a_get_ts(a, field, text, encoding);
}

static void pfa_sub_a_destroy_i(void *p)
{
    FrtAnalyzer *a = (FrtAnalyzer *) p;
    frt_a_deref(a);
}

void frt_pfa_add_field(FrtAnalyzer *self,
                   FrtSymbol field,
                   FrtAnalyzer *analyzer)
{
    frt_h_set(PFA(self)->dict, (void *)field, analyzer);
}

FrtAnalyzer *frt_per_field_analyzer_new(FrtAnalyzer *default_a)
{
    FrtAnalyzer *a = (FrtAnalyzer *)frt_ecalloc(sizeof(FrtPerFieldAnalyzer));

    PFA(a)->default_a = default_a;
    PFA(a)->dict = frt_h_new_ptr(&pfa_sub_a_destroy_i);

    a->destroy_i = &pfa_destroy_i;
    a->get_ts    = pfa_get_ts;
    a->ref_cnt   = 1;

    return a;
}
