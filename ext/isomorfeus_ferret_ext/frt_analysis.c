#include "frt_analysis.h"
#include "frt_hash.h"
#include "libstemmer.h"
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include "frt_internal.h"
#include "frt_scanner.h"

/****************************************************************************
 *
 * Token
 *
 ****************************************************************************/

FrtToken *frt_tk_set(FrtToken *tk,
                     char *text, int tlen, off_t start, off_t end, int pos_inc)
{
    if (tlen >= FRT_MAX_WORD_SIZE) {
        tlen = FRT_MAX_WORD_SIZE - 1;
    }
    memcpy(tk->text, text, sizeof(char) * tlen);
    tk->text[tlen] = '\0';
    tk->len = tlen;
    tk->start = start;
    tk->end = end;
    tk->pos_inc = pos_inc;
    return tk;
}

static FrtToken *tk_set_ts(FrtToken *tk, char *start, char *end,
                               char *text, int pos_inc)
{
    return frt_tk_set(tk, start, (int)(end - start),
                  (off_t)(start - text), (off_t)(end - text), pos_inc);
}

static FrtToken *w_tk_set(FrtToken *tk, wchar_t *text, off_t start,
                              off_t end, int pos_inc)
{
    int len = wcstombs(tk->text, text, FRT_MAX_WORD_SIZE - 1);
    tk->text[len] = '\0';
    tk->len = len;
    tk->start = start;
    tk->end = end;
    tk->pos_inc = pos_inc;
    return tk;
}

void frt_tk_destroy(void *p)
{
    free(p);
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

static FrtTokenStream *ts_reset(FrtTokenStream *ts, char *text)
{
    ts->t = ts->text = text;
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

static int mb_next_char(wchar_t *wchr, const char *s, mbstate_t *state)
{
    int num_bytes;
    if ((num_bytes = (int)mbrtowc(wchr, s, MB_CUR_MAX, state)) < 0) {
        const char *t = s;
        do {
            t++;
            FRT_ZEROSET(state, mbstate_t);
            num_bytes = (int)mbrtowc(wchr, t, MB_CUR_MAX, state);
        } while ((num_bytes < 0) && (*t != 0));
        num_bytes = t - s;
        if (*t == 0) *wchr = 0;
    }
    return num_bytes;
}

static FrtTokenStream *mb_ts_reset(FrtTokenStream *ts, char *text)
{
    FRT_ZEROSET(&(MBTS(ts)->state), mbstate_t);
    ts_reset(ts, text);
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

static FrtTokenStream *a_standard_get_ts(FrtAnalyzer *a,
                                      FrtSymbol field,
                                      char *text)
{
    FrtTokenStream *ts;
    (void)field;
    ts = frt_ts_clone(a->current_ts);
    return ts->reset(ts, text);
}

FrtAnalyzer *frt_analyzer_new(FrtTokenStream *ts,
                       void (*destroy_i)(FrtAnalyzer *a),
                       FrtTokenStream *(*get_ts)(FrtAnalyzer *a,
                                              FrtSymbol field,
                                              char *text))
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

        return frt_tk_set(&(CTS(ts)->token), ts->text, len, 0, len, 1);
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

/****************************************************************************
 *
 * Whitespace
 *
 ****************************************************************************/

/*
 * WhitespaceTokenizer
 */
static FrtToken *wst_next(FrtTokenStream *ts)
{
    char *t = ts->t;
    char *start;

    while (*t != '\0' && isspace(*t)) {
        t++;
    }

    if (*t == '\0') {
        return NULL;
    }

    start = t;
    while (*t != '\0' && !isspace(*t)) {
        t++;
    }

    ts->t = t;
    return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
}

FrtTokenStream *frt_whitespace_tokenizer_new()
{
    FrtTokenStream *ts = cts_new();
    ts->next = &wst_next;
    return ts;
}

/*
 * Multi-byte WhitespaceTokenizer
 */
static FrtToken *mb_wst_next(FrtTokenStream *ts)
{
    int i;
    char *start;
    char *t = ts->t;
    wchar_t wchr;
    mbstate_t *state = &(MBTS(ts)->state);

    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && iswspace(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    if (wchr == 0) {
        return NULL;
    }

    start = t;
    t += i;
    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && !iswspace(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    ts->t = t;
    return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
}

/*
 * Lowercasing Multi-byte WhitespaceTokenizer
 */
static FrtToken *mb_wst_next_lc(FrtTokenStream *ts)
{
    int i;
    char *start;
    char *t = ts->t;
    wchar_t wchr;
    wchar_t wbuf[FRT_MAX_WORD_SIZE + 1], *w, *w_end;
    mbstate_t *state = &(MBTS(ts)->state);

    w = wbuf;
    w_end = &wbuf[FRT_MAX_WORD_SIZE];

    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && iswspace(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    if (wchr == 0) {
        return NULL;
    }

    start = t;
    t += i;
    *w++ = towlower(wchr);
    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && !iswspace(wchr)) {
        if (w < w_end) {
            *w++ = towlower(wchr);
        }
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    *w = 0;
    ts->t = t;
    return w_tk_set(&(CTS(ts)->token), wbuf, (off_t)(start - ts->text),
                    (off_t)(t - ts->text), 1);
}

FrtTokenStream *frt_mb_whitespace_tokenizer_new(bool lowercase)
{
    FrtTokenStream *ts = mb_ts_new();
    ts->next = lowercase ? &mb_wst_next_lc : &mb_wst_next;
    return ts;
}

/*
 * WhitespaceAnalyzers
 */
FrtAnalyzer *frt_whitespace_analyzer_new(bool lowercase)
{
    FrtTokenStream *ts;
    if (lowercase) {
        ts = lowercase_filter_new(frt_whitespace_tokenizer_new());
    }
    else {
        ts = frt_whitespace_tokenizer_new();
    }
    return frt_analyzer_new(ts, NULL, NULL);
}

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
 * LetterTokenizer
 */
static FrtToken *lt_next(FrtTokenStream *ts)
{
    char *start;
    char *t = ts->t;

    while (*t != '\0' && !isalpha(*t)) {
        t++;
    }

    if (*t == '\0') {
        return NULL;
    }

    start = t;
    while (*t != '\0' && isalpha(*t)) {
        t++;
    }

    ts->t = t;
    return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
}

FrtTokenStream *letter_tokenizer_new()
{
    FrtTokenStream *ts = cts_new();
    ts->next = &lt_next;
    return ts;
}

/*
 * Multi-byte LetterTokenizer
 */
static FrtToken *mb_lt_next(FrtTokenStream *ts)
{
    int i;
    char *start;
    char *t = ts->t;
    wchar_t wchr;
    mbstate_t *state = &(MBTS(ts)->state);

    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && !iswalpha(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }

    if (wchr == 0) {
        return NULL;
    }

    start = t;
    t += i;
    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && iswalpha(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    ts->t = t;
    return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
}

/*
 * Lowercasing Multi-byte LetterTokenizer
 */
static FrtToken *mb_lt_next_lc(FrtTokenStream *ts)
{
    int i;
    char *start;
    char *t = ts->t;
    wchar_t wchr;
    wchar_t wbuf[FRT_MAX_WORD_SIZE + 1], *w, *w_end;
    mbstate_t *state = &(MBTS(ts)->state);

    w = wbuf;
    w_end = &wbuf[FRT_MAX_WORD_SIZE];

    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && !iswalpha(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    if (wchr == 0) {
        return NULL;
    }

    start = t;
    t += i;
    *w++ = towlower(wchr);
    i = mb_next_char(&wchr, t, state);
    while (wchr != 0 && iswalpha(wchr)) {
        if (w < w_end) {
            *w++ = towlower(wchr);
        }
        t += i;
        i = mb_next_char(&wchr, t, state);
    }
    *w = 0;
    ts->t = t;
    return w_tk_set(&(CTS(ts)->token), wbuf, (off_t)(start - ts->text),
                    (off_t)(t - ts->text), 1);
}

FrtTokenStream *frt_mb_letter_tokenizer_new(bool lowercase)
{
    FrtTokenStream *ts = mb_ts_new();
    ts->next = lowercase ? &mb_lt_next_lc : &mb_lt_next;
    return ts;
}

/*
 * LetterAnalyzers
 */
FrtAnalyzer *letter_analyzer_new(bool lowercase)
{
    FrtTokenStream *ts;
    if (lowercase) {
        ts = lowercase_filter_new(letter_tokenizer_new());
    }
    else {
        ts = letter_tokenizer_new();
    }
    return frt_analyzer_new(ts, NULL, NULL);
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
    FrtStandardTokenizer *std_tz = STDTS(ts);
    const char *start = NULL;
    const char *end = NULL;
    int len;
    FrtToken *tk = &(CTS(ts)->token);

    switch (std_tz->type) {
        case FRT_STT_ASCII:
            frt_std_scan(ts->t, tk->text, sizeof(tk->text) - 1,
                         &start, &end, &len);
            break;
        case FRT_STT_MB:
            frt_std_scan_mb(ts->t, tk->text, sizeof(tk->text) - 1,
                            &start, &end, &len);
            break;
        case FRT_STT_UTF8:
            frt_std_scan_utf8(ts->t, tk->text, sizeof(tk->text) - 1,
                              &start, &end, &len);
            break;
    }

    if (len == 0)
        return NULL;

    ts->t       = (char *)end;
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

    ts->clone_i     = &std_ts_clone_i;
    ts->next        = &std_next;

    return ts;
}

FrtTokenStream *frt_standard_tokenizer_new()
{
    FrtTokenStream *ts = std_ts_new();
    STDTS(ts)->type = FRT_STT_ASCII;
    return ts;
}

FrtTokenStream *frt_mb_standard_tokenizer_new()
{
    FrtTokenStream *ts = std_ts_new();
    STDTS(ts)->type = FRT_STT_MB;
    return ts;
}

FrtTokenStream *frt_utf8_standard_tokenizer_new()
{
    FrtTokenStream *ts = std_ts_new();
    STDTS(ts)->type = FRT_STT_UTF8;
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
static int legacy_std_get_alpha(FrtTokenStream *ts, char *token)
{
    int i = 0;
    char *t = ts->t;
    while (t[i] != '\0' && isalnum(t[i])) {
        if (i < FRT_MAX_WORD_SIZE) {
            token[i] = t[i];
        }
        i++;
    }
    return i;
}

static int mb_legacy_std_get_alpha(FrtTokenStream *ts, char *token)
{
    char *t = ts->t;
    wchar_t wchr;
    int i;
    mbstate_t state; FRT_ZEROSET(&state, mbstate_t);

    i = mb_next_char(&wchr, t, &state);

    while (wchr != 0 && iswalnum(wchr)) {
        t += i;
        i = mb_next_char(&wchr, t, &state);
    }

    i = (int)(t - ts->t);
    if (i > FRT_MAX_WORD_SIZE) {
        i = FRT_MAX_WORD_SIZE - 1;
    }
    memcpy(token, ts->t, i);
    return i;
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

static bool legacy_std_is_tok_char(char *c)
{
    if (isspace(*c)) {
        return false;           /* most common so check first. */
    }
    if (isalnum(*c) || isnumpunc(*c) || *c == '&' ||
        *c == '@' || *c == '\'' || *c == ':') {
        return true;
    }
    return false;
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

static int legacy_std_get_apostrophe(char *input)
{
    char *t = input;

    while (isalpha(*t) || *t == '\'') {
        t++;
    }

    return (int)(t - input);
}

static int mb_legacy_std_get_apostrophe(char *input)
{
    char *t = input;
    wchar_t wchr;
    int i;
    mbstate_t state; FRT_ZEROSET(&state, mbstate_t);

    i = mb_next_char(&wchr, t, &state);

    while (iswalpha(wchr) || wchr == L'\'') {
        t += i;
        i = mb_next_char(&wchr, t, &state);
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

static bool legacy_std_advance_to_start(FrtTokenStream *ts)
{
    char *t = ts->t;
    while (*t != '\0' && !isalnum(*t)) {
        if (isnumpunc(*t) && isdigit(t[1])) break;
        t++;
    }

    ts->t = t;

    return (*t != '\0');
}

static bool mb_legacy_std_advance_to_start(FrtTokenStream *ts)
{
    int i;
    wchar_t wchr;
    mbstate_t state; FRT_ZEROSET(&state, mbstate_t);

    i = mb_next_char(&wchr, ts->t, &state);

    while (wchr != 0 && !iswalnum(wchr)) {
        if (isnumpunc(*ts->t) && isdigit(ts->t[1])) break;
        ts->t += i;
        i = mb_next_char(&wchr, ts->t, &state);
    }

    return (wchr != 0);
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


    if (!std_tz->advance_to_start(ts)) {
        return NULL;
    }

    start = t = ts->t;
    token_i = std_tz->get_alpha(ts, token);
    t += token_i;

    if (!std_tz->is_tok_char(t)) {
        /* very common case, ie a plain word, so check and return */
        ts->t = t;
        return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
    }

    if (*t == '\'') {       /* apostrophe case. */
        t += std_tz->get_apostrophe(t);
        ts->t = t;
        len = (int)(t - start);
        /* strip possesive */
        if ((t[-1] == 's' || t[-1] == 'S') && t[-2] == '\'') {
            t -= 2;
            tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
            CTS(ts)->token.end += 2;
        }
        else if (t[-1] == '\'') {
            t -= 1;
            tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
            CTS(ts)->token.end += 1;
        }
        else {
            tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
        }

        return &(CTS(ts)->token);
    }

    if (*t == '&') {        /* apostrophe case. */
        t += legacy_std_get_company_name(t);
        ts->t = t;
        return tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
    }

    if ((isdigit(*start) || isnumpunc(*start))       /* possibly a number */
        && ((len = legacy_std_get_number(start)) > 0)) {
        num_end = start + len;
        if (!std_tz->is_tok_char(num_end)) { /* won't find a longer token */
            ts->t = num_end;
            return tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1);
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
                      (off_t)(ts->t - ts->text), 1);
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
                   (off_t)(t - ts->text), 1);
        }
        else { /* just return the url as is */
            tk_set_ts(&(CTS(ts)->token), start, t, ts->text, 1);
        }
    }
    else {                  /* return the number */
        ts->t = num_end;
        tk_set_ts(&(CTS(ts)->token), start, num_end, ts->text, 1);
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

    ts->clone_i     = &legacy_std_ts_clone_i;
    ts->next        = &legacy_std_next;

    return ts;
}

FrtTokenStream *legacy_standard_tokenizer_new()
{
    FrtTokenStream *ts = legacy_std_ts_new();

    LSTDTS(ts)->advance_to_start = &legacy_std_advance_to_start;
    LSTDTS(ts)->get_alpha        = &legacy_std_get_alpha;
    LSTDTS(ts)->is_tok_char      = &legacy_std_is_tok_char;
    LSTDTS(ts)->get_apostrophe   = &legacy_std_get_apostrophe;

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

FrtTokenStream *filter_clone_size(FrtTokenStream *ts, size_t size)
{
    FrtTokenStream *ts_new = frt_ts_clone_size(ts, size);
    TkFilt(ts_new)->sub_ts = TkFilt(ts)->sub_ts->clone_i(TkFilt(ts)->sub_ts);
    return ts_new;
}

static FrtTokenStream *filter_clone_i(FrtTokenStream *ts)
{
    return filter_clone_size(ts, sizeof(FrtTokenFilter));
}

static FrtTokenStream *filter_reset(FrtTokenStream *ts, char *text)
{
    TkFilt(ts)->sub_ts->reset(TkFilt(ts)->sub_ts, text);
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
    h_destroy(StopFilt(ts)->words);
    filter_destroy_i(ts);
}

static FrtTokenStream *sf_clone_i(FrtTokenStream *orig_ts)
{
    FrtTokenStream *new_ts = filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
    FRT_REF(StopFilt(new_ts)->words);
    return new_ts;
}

static FrtToken *sf_next(FrtTokenStream *ts)
{
    int pos_inc = 0;
    FrtHash *words = StopFilt(ts)->words;
    FrtTokenFilter *tf = TkFilt(ts);
    FrtToken *tk = tf->sub_ts->next(tf->sub_ts);

    while ((tk != NULL) && (h_get(words, tk->text) != NULL)) {
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
    FrtHash *word_table = h_new_str(&free, (free_ft) NULL);
    FrtTokenStream *ts = tf_new(FrtStopFilter, sub_ts);

    for (i = 0; i < len; i++) {
        word = frt_estrdup(words[i]);
        h_set(word_table, word, word);
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
    FrtHash *word_table = h_new_str(&free, (free_ft) NULL);
    FrtTokenStream *ts = tf_new(FrtStopFilter, sub_ts);

    while (*words) {
        word = frt_estrdup(*words);
        h_set(word_table, word, word);
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
    FrtTokenStream *new_ts = filter_clone_size(orig_ts, sizeof(FrtMappingFilter));
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

static FrtTokenStream *mf_reset(FrtTokenStream *ts, char *text)
{
    FrtMultiMapper *mm = MFilt(ts)->mapper;
    if (mm->d_size == 0) {
        frt_mulmap_compile(MFilt(ts)->mapper);
    }
    filter_reset(ts, text);
    return ts;
}

FrtTokenStream *frt_mapping_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts   = tf_new(FrtMappingFilter, sub_ts);
    MFilt(ts)->mapper = frt_mulmap_new();
    ts->next          = &mf_next;
    ts->destroy_i     = &mf_destroy_i;
    ts->clone_i       = &mf_clone_i;
    ts->reset         = &mf_reset;
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
    FrtTokenStream *new_ts = filter_clone_size(orig_ts, sizeof(FrtHyphenFilter));
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

FrtTokenStream *hyphen_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtHyphenFilter, sub_ts);
    ts->next        = &hf_next;
    ts->clone_i     = &hf_clone_i;
    return ts;
}

/****************************************************************************
 * LowerCaseFilter
 ****************************************************************************/


static FrtToken *mb_lcf_next(FrtTokenStream *ts)
{
    wchar_t wbuf[FRT_MAX_WORD_SIZE + 1], *wchr;
    FrtToken *tk = TkFilt(ts)->sub_ts->next(TkFilt(ts)->sub_ts);
    int x;
    wbuf[FRT_MAX_WORD_SIZE] = 0;

    if (tk == NULL) {
        return tk;
    }

    if ((x=mbstowcs(wbuf, tk->text, FRT_MAX_WORD_SIZE)) <= 0) return tk;
    wchr = wbuf;
    while (*wchr != 0) {
        *wchr = towlower(*wchr);
        wchr++;
    }
    tk->len = wcstombs(tk->text, wbuf, FRT_MAX_WORD_SIZE);
    if (tk->len <= 0) {
        strcpy(tk->text, "BAD_DATA");
        tk->len = 8;
    }
    tk->text[tk->len] = '\0';
    return tk;
}

FrtTokenStream *frt_mb_lowercase_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtTokenFilter, sub_ts);
    ts->next = &mb_lcf_next;
    return ts;
}

static FrtToken *lcf_next(FrtTokenStream *ts)
{
    int i = 0;
    FrtToken *tk = TkFilt(ts)->sub_ts->next(TkFilt(ts)->sub_ts);
    if (tk == NULL) {
        return tk;
    }
    while (tk->text[i] != '\0') {
        tk->text[i] = tolower(tk->text[i]);
        i++;
    }
    return tk;
}

FrtTokenStream *lowercase_filter_new(FrtTokenStream *sub_ts)
{
    FrtTokenStream *ts = tf_new(FrtTokenFilter, sub_ts);
    ts->next = &lcf_next;
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
    FrtTokenStream *new_ts      = filter_clone_size(orig_ts, sizeof(FrtStemFilter));
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

FrtAnalyzer *frt_standard_analyzer_new_with_words_len(const char **words, int len,
                                               bool lowercase)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    if (lowercase) {
        ts = lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words_len(ts, words, len));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_standard_analyzer_new_with_words(const char **words,
                                           bool lowercase)
{
    FrtTokenStream *ts = frt_standard_tokenizer_new();
    if (lowercase) {
        ts = lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_mb_standard_analyzer_new_with_words(const char **words,
                                              bool lowercase)
{
    FrtTokenStream *ts = frt_mb_standard_tokenizer_new();
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_utf8_standard_analyzer_new_with_words(const char **words,
                                              bool lowercase)
{
    FrtTokenStream *ts = frt_utf8_standard_tokenizer_new();
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_standard_analyzer_new(bool lowercase)
{
    return frt_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                            lowercase);
}

FrtAnalyzer *frt_mb_standard_analyzer_new(bool lowercase)
{
    return frt_mb_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                               lowercase);
}

FrtAnalyzer *frt_utf8_standard_analyzer_new(bool lowercase)
{
    return frt_utf8_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                                 lowercase);
}

/****************************************************************************
 * Legacy
 ****************************************************************************/

FrtAnalyzer *legacy_standard_analyzer_new_with_words_len(const char **words, int len,
                                                      bool lowercase)
{
    FrtTokenStream *ts = legacy_standard_tokenizer_new();
    if (lowercase) {
        ts = lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words_len(ts, words, len));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *legacy_standard_analyzer_new_with_words(const char **words,
                                                  bool lowercase)
{
    FrtTokenStream *ts = legacy_standard_tokenizer_new();
    if (lowercase) {
        ts = lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *frt_mb_legacy_standard_analyzer_new_with_words(const char **words,
                                                     bool lowercase)
{
    FrtTokenStream *ts = frt_mb_legacy_standard_tokenizer_new();
    if (lowercase) {
        ts = frt_mb_lowercase_filter_new(ts);
    }
    ts = hyphen_filter_new(frt_stop_filter_new_with_words(ts, words));
    return frt_analyzer_new(ts, NULL, NULL);
}

FrtAnalyzer *legacy_standard_analyzer_new(bool lowercase)
{
    return legacy_standard_analyzer_new_with_words(FRT_FULL_ENGLISH_STOP_WORDS,
                                                   lowercase);
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
    h_destroy(PFA(self)->dict);

    frt_a_deref(PFA(self)->default_a);
    free(self);
}

static FrtTokenStream *pfa_get_ts(FrtAnalyzer *self,
                               FrtSymbol field, char *text)
{
    FrtAnalyzer *a = (FrtAnalyzer *)h_get(PFA(self)->dict, field);
    if (a == NULL) {
        a = PFA(self)->default_a;
    }
    return frt_a_get_ts(a, field, text);
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
    h_set(PFA(self)->dict, field, analyzer);
}

FrtAnalyzer *frt_per_field_analyzer_new(FrtAnalyzer *default_a)
{
    FrtAnalyzer *a = (FrtAnalyzer *)frt_ecalloc(sizeof(FrtPerFieldAnalyzer));

    PFA(a)->default_a = default_a;
    PFA(a)->dict = h_new_str(NULL, &pfa_sub_a_destroy_i);

    a->destroy_i = &pfa_destroy_i;
    a->get_ts    = pfa_get_ts;
    a->ref_cnt   = 1;

    return a;
}
