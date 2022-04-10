%{
/*****************************************************************************
 * QueryParser
 * ===========
 *
 * Brief Overview
 * --------------
 *
 * === Creating a QueryParser
 *
 *  +qp_new+ allocates a new QueryParser and assigns three very important
 *  HashSets; +qp->def_fields+, +qp->tkz_fields+ and +qp->all_fields+. The
 *  query language allows you to assign a field or a set of fields to each
 *  part of the query.
 *
 *    - +qp->def_fields+ is the set of fields that a query is applied to by
 *      default when no fields are specified.
 *    - +qp->all_fields+ is the set of fields that gets searched when the user
 *      requests a search of all fields.
 *    - +qp->tkz_fields+ is the set of fields that gets tokenized before being
 *      added to the query parser.
 *
 * === qp_parse
 *
 *  The main QueryParser method is +qp_parse+. It gets called with a the query
 *  string and returns a Query object which can then be passed to the
 *  IndexSearcher. The first thing it does is to clean the query string if
 *  +qp->clean_str+ is set to true. The cleaning is done with the
 *  +qp_clean_str+.
 *
 *  It then calls the yacc parser which will set +qp->result+ to the parsed
 *  query. If parsing fails in any way, +qp->result+ should be set to NULL, in
 *  which case qp_parse does one of two things depending on the value of
 *  +qp->handle_parse_errors+;
 *
 *    - If it is set to true, qp_parse attempts to do a very basic parsing of
 *      the query by ignoring all special characters and parsing the query as
 *      a plain boolean query.
 *    - If it is set to false, qp_parse will raise a PARSE_ERROR and hopefully
 *      free all allocated memory.
 *
 * === The Lexer
 *
 *  +yylex+ is the lexing method called by the QueryParser. It breaks the
 *  query up into special characters;
 *
 *      ( "&:()[]{}!\"~^|<>=*?+-" )
 *
 *  and tokens;
 *
 *    - QWRD
 *    - WILD_STR
 *    - AND['AND', '&&']
 *    - OR['OR', '||']
 *    - REQ['REQ', '+']
 *    - NOT['NOT', '-', '~']
 *
 *  QWRD tokens are query word tokens which are made up of characters other
 *  than the special characters. They can also contain special characters when
 *  escaped with a backslash '\'. WILD_STR is the same as QWRD except that it
 *  may also contain '?' and '*' characters.
 *
 * === The Parser
 *
 *  For a better understanding of the how the query parser works, it is a good
 *  idea to study the Ferret Query Language (FQL) described below. Once you
 *  understand FQL the one tricky part that needs to be mentioned is how
 *  fields are handled. This is where +qp->def_fields+ and +qp->all_fields
 *  come into play. When no fields are specified then the default fields are
 *  used. The '*:' field specifier will search all fields contained in the
 *  all_fields set.  Otherwise all fields specified in the field descripter
 *  separated by '|' will be searched. For example 'title|content:' will
 *  search the title and content fields. When fields are specified like this,
 *  the parser will push the fields onto a stack and all queries modified by
 *  the field specifier will be applied to the fields on top of the stack.
 *  The parser uses the FLDS macro to handle the current fields. It takes the
 *  current query building function in the parser and calls it for all the
 *  current search fields (on top of the stack).
 *
 * Ferret Query Language (FQL)
 * ---------------------------
 *
 * FIXME to be continued...
 *****************************************************************************/

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "frt_global.h"
#include "frt_except.h"
#include "frt_search.h"
#include "frt_array.h"
#include <ruby/encoding.h>

extern rb_encoding *utf8_encoding;
extern int utf8_mbmaxlen;

typedef struct Phrase {
    int             size;
    int             capa;
    int             pos_inc;
    FrtPhrasePosition *positions;
} Phrase;

#define BCA_INIT_CAPA 4
typedef struct FrtBCArray {
    int size;
    int capa;
    FrtBooleanClause **clauses;
} FrtBCArray;

float frt_qp_default_fuzzy_min_sim = 0.5;
int frt_qp_default_fuzzy_pre_len = 0;

%}
%union {
    FrtQuery *query;
    FrtBooleanClause *bcls;
    FrtBCArray *bclss;
    FrtHashSet *hashset;
    Phrase *phrase;
    char *str;
}
%{
static int yylex(YYSTYPE *lvalp, FrtQParser *qp);
static int yyerror(FrtQParser *qp, rb_encoding *encoding, char const *msg);

#define PHRASE_INIT_CAPA 4
static FrtQuery *get_bool_q(FrtBCArray *bca);

static FrtBCArray *first_cls(FrtBooleanClause *boolean_clause);
static FrtBCArray *add_and_cls(FrtBCArray *bca, FrtBooleanClause *clause);
static FrtBCArray *add_or_cls(FrtBCArray *bca, FrtBooleanClause *clause);
static FrtBCArray *add_default_cls(FrtQParser *qp, FrtBCArray *bca, FrtBooleanClause *clause);
static void bca_destroy(FrtBCArray *bca);

static FrtBooleanClause *get_bool_cls(FrtQuery *q, FrtBCType occur);

static FrtQuery *get_term_q(FrtQParser *qp, FrtSymbol field, char *word, rb_encoding *encoding);
static FrtQuery *get_fuzzy_q(FrtQParser *qp, FrtSymbol field, char *word, char *slop, rb_encoding *encoding);
static FrtQuery *get_wild_q(FrtQParser *qp, FrtSymbol field, char *pattern, rb_encoding *encoding);

static FrtHashSet *first_field(FrtQParser *qp, const char *field_name);
static FrtHashSet *add_field(FrtQParser *qp, const char *field_name);

static FrtQuery *get_phrase_q(FrtQParser *qp, Phrase *phrase, char *slop, rb_encoding *encoding);

static Phrase *ph_first_word(char *word);
static Phrase *ph_add_word(Phrase *self, char *word);
static Phrase *ph_add_multi_word(Phrase *self, char *word);
static void ph_destroy(Phrase *self);

static FrtQuery *get_r_q(FrtQParser *qp, FrtSymbol field, char *from, char *to, bool inc_lower, bool inc_upper, rb_encoding *encoding);

static void qp_push_fields(FrtQParser *self, FrtHashSet *fields, bool destroy);
static void qp_pop_fields(FrtQParser *self);

/**
 * +FLDS+ calls +func+ for all fields on top of the field stack. +func+
 * must return a query. If there is more than one field on top of FieldStack
 * then +FLDS+ will combing all the queries returned by +func+ into a single
 * BooleanQuery which it than assigns to +q+. If there is only one field, the
 * return value of +func+ is assigned to +q+ directly.
 */
#define FLDS(q, func) do {\
    FRT_TRY {\
        FrtSymbol field;\
        if (qp->fields->size == 0) {\
            q = NULL;\
        } else if (qp->fields->size == 1) {\
            field = (FrtSymbol)qp->fields->first->elem;\
            q = func;\
        } else {\
            FrtQuery *volatile sq; FrtHashSetEntry *volatile hse;\
            q = frt_bq_new_max(false, qp->max_clauses);\
            for (hse = qp->fields->first; hse; hse = hse->next) {\
                field = (FrtSymbol)hse->elem;\
                sq = func;\
                FRT_TRY\
                  if (sq) frt_bq_add_query_nr(q, sq, FRT_BC_SHOULD);\
                FRT_XCATCHALL\
                  if (sq) frt_q_deref(sq);\
                FRT_XENDTRY\
            }\
            if (((FrtBooleanQuery *)q)->clause_cnt == 0) {\
                frt_q_deref(q);\
                q = NULL;\
            }\
        }\
    } FRT_XCATCHALL\
        qp->destruct = true;\
        FRT_HANDLED();\
    FRT_XENDTRY\
    if (qp->destruct && !qp->recovering && q) {frt_q_deref(q); q = NULL;}\
} while (0)

#define Y if (qp->destruct) goto yyerrorlab;
#define T FRT_TRY
#define E\
  FRT_XCATCHALL\
    qp->destruct = true;\
    FRT_HANDLED();\
  FRT_XENDTRY\
  if (qp->destruct) Y;
%}
%expect 1
%define api.pure full
%parse-param { FrtQParser *qp } { rb_encoding *encoding }
%lex-param   { FrtQParser *qp }
%token <str>    QWRD WILD_STR
%type <query>   q bool_q boosted_q term_q wild_q field_q phrase_q range_q
%type <bcls>    bool_cls
%type <bclss>   bool_clss
%type <hashset> field
%type <phrase>  ph_words
%nonassoc LOW
%left AND OR
%nonassoc REQ NOT
%left ':'
%nonassoc HIGH
%destructor { if ($$ && qp->destruct) frt_q_deref($$); } q bool_q boosted_q term_q wild_q field_q phrase_q range_q
%destructor { if ($$ && qp->destruct) frt_bc_deref($$); } bool_cls
%destructor { if ($$ && qp->destruct) bca_destroy($$); } bool_clss
%destructor { if ($$ && qp->destruct) ph_destroy($$); } ph_words
%%
bool_q    : /* Nothing */             {   qp->result = $$ = NULL; }
          | bool_clss                 { T qp->result = $$ = get_bool_q($1); E }
          ;
bool_clss : bool_cls                  { T $$ = first_cls($1); E }
          | bool_clss AND bool_cls    { T $$ = add_and_cls($1, $3); E }
          | bool_clss OR bool_cls     { T $$ = add_or_cls($1, $3); E }
          | bool_clss bool_cls        { T $$ = add_default_cls(qp, $1, $2); E }
          ;
bool_cls  : REQ boosted_q             { T $$ = get_bool_cls($2, FRT_BC_MUST); E }
          | NOT boosted_q             { T $$ = get_bool_cls($2, FRT_BC_MUST_NOT); E }
          | boosted_q                 { T $$ = get_bool_cls($1, FRT_BC_SHOULD); E }
          ;
boosted_q : q
          | q '^' QWRD                { T if ($1) sscanf($3,"%f",&($1->boost));  $$=$1; E }
          ;
q         : term_q
          | '(' ')'                   { T $$ = frt_bq_new_max(true, qp->max_clauses); E }
          | '(' bool_clss ')'         { T $$ = get_bool_q($2); E }
          | field_q
          | phrase_q
          | range_q
          | wild_q
          ;
term_q    : QWRD                      { FLDS($$, get_term_q(qp, field, $1, encoding)); Y}
          | QWRD '~' QWRD %prec HIGH  { FLDS($$, get_fuzzy_q(qp, field, $1, $3, encoding)); Y}
          | QWRD '~' %prec LOW        { FLDS($$, get_fuzzy_q(qp, field, $1, NULL, encoding)); Y}
          ;
wild_q    : WILD_STR                  { FLDS($$, get_wild_q(qp, field, $1, encoding)); Y}
          ;
field_q   : field ':' q { qp_pop_fields(qp); }
                                      { $$ = $3; }
          | '*' { qp_push_fields(qp, qp->all_fields, false); } ':' q { qp_pop_fields(qp); }
                                      { $$ = $4; }
          ;
field     : QWRD                      { $$ = first_field(qp, $1); }
          | field '|' QWRD            { $$ = add_field(qp, $3);}
          ;
phrase_q  : '"' ph_words '"'          { $$ = get_phrase_q(qp, $2, NULL, encoding); }
          | '"' ph_words '"' '~' QWRD { $$ = get_phrase_q(qp, $2, $5, encoding); }
          | '"' '"'                   { $$ = NULL; }
          | '"' '"' '~' QWRD          { $$ = NULL; (void)$4;}
          ;
ph_words  : QWRD              { $$ = ph_first_word($1); }
          | '<' '>'           { $$ = ph_first_word(NULL); }
          | ph_words QWRD     { $$ = ph_add_word($1, $2); }
          | ph_words '<' '>'  { $$ = ph_add_word($1, NULL); }
          | ph_words '|' QWRD { $$ = ph_add_multi_word($1, $3);  }
          ;
range_q   : '[' QWRD QWRD ']' { FLDS($$, get_r_q(qp, field, $2,  $3,  true,  true,  encoding)); Y}
          | '[' QWRD QWRD '}' { FLDS($$, get_r_q(qp, field, $2,  $3,  true,  false, encoding)); Y}
          | '{' QWRD QWRD ']' { FLDS($$, get_r_q(qp, field, $2,  $3,  false, true,  encoding)); Y}
          | '{' QWRD QWRD '}' { FLDS($$, get_r_q(qp, field, $2,  $3,  false, false, encoding)); Y}
          | '<' QWRD '}'      { FLDS($$, get_r_q(qp, field, NULL,$2,  false, false, encoding)); Y}
          | '<' QWRD ']'      { FLDS($$, get_r_q(qp, field, NULL,$2,  false, true,  encoding)); Y}
          | '[' QWRD '>'      { FLDS($$, get_r_q(qp, field, $2,  NULL,true,  false, encoding)); Y}
          | '{' QWRD '>'      { FLDS($$, get_r_q(qp, field, $2,  NULL,false, false, encoding)); Y}
          | '<' QWRD          { FLDS($$, get_r_q(qp, field, NULL,$2,  false, false, encoding)); Y}
          | '<' '=' QWRD      { FLDS($$, get_r_q(qp, field, NULL,$3,  false, true,  encoding)); Y}
          | '>' '='  QWRD     { FLDS($$, get_r_q(qp, field, $3,  NULL,true,  false, encoding)); Y}
          | '>' QWRD          { FLDS($$, get_r_q(qp, field, $2,  NULL,false, false, encoding)); Y}
          ;
%%

static const char *special_char = "&:()[]{}!\"~^|<>=*?+-";
static const char *not_word =   " \t()[]{}!\"~^|<>=";

/**
 * +get_word+ gets the next query-word from the query string. A query-word is
 * basically a string of non-special or escaped special characters. It is
 * FrtAnalyzer agnostic. It is up to the get_*_q methods to tokenize the word and
 * turn it into a +Query+. See the documentation for each get_*_q method to
 * see how it handles tokenization.
 *
 * Note that +get_word+ is also responsible for returning field names and
 * matching the special tokens 'AND', 'NOT', 'REQ' and 'OR'.
 */
static int get_word(YYSTYPE *lvalp, FrtQParser *qp)
{
    bool is_wild = false;
    int len;
    char c;
    char *buf = qp->buf[qp->buf_index];
    char *bufp = buf;
    qp->buf_index = (qp->buf_index + 1) % FRT_QP_CONC_WORDS;

    if (qp->dynbuf) {
        free(qp->dynbuf);
        qp->dynbuf = NULL;
    }

    qp->qstrp--; /* need to back up one character */

    while (!strchr(not_word, (c = *qp->qstrp++))) {
        switch (c) {
            case '\\':
                if ((c = *qp->qstrp) == '\0') {
                    *bufp++ = '\\';
                }
                else {
                    *bufp++ = c;
                    qp->qstrp++;
                }
                break;
            case ':':
                if ((*qp->qstrp) == ':') {
                    qp->qstrp++;
                    *bufp++ = ':';
                    *bufp++ = ':';
                }
                else {
                   goto get_word_done;
                }
                break;
            case '*': case '?':
                is_wild = true;
                /* fall through */
            default:
                *bufp++ = c;
        }
        /* we've exceeded the static buffer. switch to the dynamic one. The
         * dynamic buffer is allocated enough space to hold the whole query
         * string so it's capacity doesn't need to be checked again once
         * allocated. */
        if (!qp->dynbuf && ((bufp - buf) == FRT_MAX_WORD_SIZE)) {
            qp->dynbuf = FRT_ALLOC_AND_ZERO_N(char, strlen(qp->qstr) + 1);
            strncpy(qp->dynbuf, buf, FRT_MAX_WORD_SIZE);
            buf = qp->dynbuf;
            bufp = buf + FRT_MAX_WORD_SIZE;
        }
    }
get_word_done:
    qp->qstrp--;
    /* check for keywords. There are only four so we have a bit of a hack
     * which just checks for all of them. */
    *bufp = '\0';
    len = (int)(bufp - buf);
    if (qp->use_keywords) {
        if (len == 3) {
            if (buf[0] == 'A' && buf[1] == 'N' && buf[2] == 'D') return AND;
            if (buf[0] == 'N' && buf[1] == 'O' && buf[2] == 'T') return NOT;
            if (buf[0] == 'R' && buf[1] == 'E' && buf[2] == 'Q') return REQ;
        }
        if (len == 2 && buf[0] == 'O' && buf[1] == 'R') return OR;
    }

    /* found a word so return it. */
    lvalp->str = buf;
    if (is_wild) {
        return WILD_STR;
    }
    return QWRD;
}

/**
 * +yylex+ is the lexing method called by the QueryParser. It breaks the
 * query up into special characters;
 *
 *     ( "&:()[]{}!\"~^|<>=*?+-" )
 *
 * and tokens;
 *
 *   - QWRD
 *   - WILD_STR
 *   - AND['AND', '&&']
 *   - OR['OR', '||']
 *   - REQ['REQ', '+']
 *   - NOT['NOT', '-', '~']
 *
 * QWRD tokens are query word tokens which are made up of characters other
 * than the special characters. They can also contain special characters when
 * escaped with a backslash '\'. WILD_STR is the same as QWRD except that it
 * may also contain '?' and '*' characters.
 *
 * If any of the special chars are seen they will usually be returned straight
 * away. The exceptions are the wild chars '*' and '?', and '&' which will be
 * treated as a plain old word character unless followed by another '&'.
 *
 * If no special characters or tokens are found then yylex delegates to
 * +get_word+ which will fetch the next query-word.
 */
static int yylex(YYSTYPE *lvalp, FrtQParser *qp)
{
    char c, nc;

    while ((c=*qp->qstrp++) == ' ' || c == '\t') {
    }

    if (c == '\0') return 0;

    if (strchr(special_char, c)) {   /* comment */
        nc = *qp->qstrp;
        switch (c) {
            case '-': case '!': return NOT;
            case '+': return REQ;
            case '*':
                if (nc == ':') return c;
                break;
            case '?':
                break;
            case '&':
                if (nc == '&') {
                    qp->qstrp++;
                    return AND;
                }
                break; /* Don't return single & character. Use in word. */
            case '|':
                if (nc == '|') {
                    qp->qstrp++;
                    return OR;
                }
            default:
                return c;
        }
    }

    return get_word(lvalp, qp);
}

/**
 * yyerror gets called if there is an parse error with the yacc parser.
 * It is responsible for clearing any memory that was allocated during the
 * parsing process.
 */
static int yyerror(FrtQParser *qp, rb_encoding *encoding, char const *msg)
{
    (void)encoding;
    qp->destruct = true;
    if (!qp->handle_parse_errors) {
        char buf[1024];
        buf[1023] = '\0';
        strncpy(buf, qp->qstr, 1023);
        if (qp->clean_str) {
            free(qp->qstr);
        }
        frt_mutex_unlock(&qp->mutex);
        snprintf(frt_xmsg_buffer, FRT_XMSG_BUFFER_SIZE,
                 "couldn't parse query ``%s''. Error message "
                 " was %s", buf, (char *)msg);
    }
    while (qp->fields_top->next != NULL) {
        qp_pop_fields(qp);
    }
    return 0;
}

#define BQ(query) ((FrtBooleanQuery *)(query))

/**
 * The QueryParser caches a tokenizer for each field so that it doesn't need
 * to allocate a new tokenizer for each term in the query. This would be quite
 * expensive as tokenizers use quite a large hunk of memory.
 *
 * This method returns the query parser for a particular field and sets it up
 * with the text to be tokenized.
 */
static FrtTokenStream *get_cached_ts(FrtQParser *qp, FrtSymbol field, char *text, rb_encoding *encoding)
{
    FrtTokenStream *ts;
    if (frt_hs_exists(qp->tokenized_fields, (void *)field)) {
        ts = (FrtTokenStream *)frt_h_get(qp->ts_cache, (void *)field);
        if (!ts) {
            ts = frt_a_get_ts(qp->analyzer, field, text, encoding);
            frt_h_set(qp->ts_cache, (void *)field, ts);
        }
        else {
            ts->reset(ts, text, encoding);
        }
    }
    else {
        ts = qp->non_tokenizer;
        ts->reset(ts, text, encoding);
    }
    return ts;
}

/**
 * Turns a BooleanClause array into a BooleanQuery. It will optimize the query
 * if 0 or 1 clauses are present to NULL or the actual query in the clause
 * respectively.
 */
static FrtQuery *get_bool_q(FrtBCArray *bca)
{
    FrtQuery *q;
    const int clause_count = bca->size;

    if (clause_count == 0) {
        q = NULL;
        free(bca->clauses);
    }
    else if (clause_count == 1) {
        FrtBooleanClause *bc = bca->clauses[0];
        if (bc->is_prohibited) {
            q = frt_bq_new(false);
            frt_bq_add_query_nr(q, bc->query, FRT_BC_MUST_NOT);
            frt_bq_add_query_nr(q, frt_maq_new(), FRT_BC_MUST);
        }
        else {
            q = bc->query;
        }
        free(bc);
        free(bca->clauses);
    }
    else {
        q = frt_bq_new(false);
        /* copy clauses into query */

        BQ(q)->clause_cnt = clause_count;
        BQ(q)->clause_capa = bca->capa;
        free(BQ(q)->clauses);
        BQ(q)->clauses = bca->clauses;
    }
    free(bca);
    return q;
}

/**
 * Base method for appending BooleanClauses to a FrtBooleanClause array. This
 * method doesn't care about the type of clause (MUST, SHOULD, MUST_NOT).
 */
static void bca_add_clause(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (bca->size >= bca->capa) {
        bca->capa <<= 1;
        FRT_REALLOC_N(bca->clauses, FrtBooleanClause *, bca->capa);
    }
    bca->clauses[bca->size] = clause;
    bca->size++;
}

/**
 * Add the first clause to a BooleanClause array. This method is also
 * responsible for allocating a new BooleanClause array.
 */
static FrtBCArray *first_cls(FrtBooleanClause *clause)
{
    FrtBCArray *bca = FRT_ALLOC_AND_ZERO(FrtBCArray);
    bca->capa = BCA_INIT_CAPA;
    bca->clauses = FRT_ALLOC_N(FrtBooleanClause *, BCA_INIT_CAPA);
    if (clause) {
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add AND clause to the BooleanClause array. The means that it will set the
 * clause being added and the previously added clause from SHOULD clauses to
 * MUST clauses. (If they are currently MUST_NOT clauses they stay as they
 * are.)
 */
static FrtBCArray *add_and_cls(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (clause) {
        if (bca->size == 1) {
            if (!bca->clauses[0]->is_prohibited) {
                frt_bc_set_occur(bca->clauses[0], FRT_BC_MUST);
            }
        }
        if (!clause->is_prohibited) {
            frt_bc_set_occur(clause, FRT_BC_MUST);
        }
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add SHOULD clause to the BooleanClause array.
 */
static FrtBCArray *add_or_cls(FrtBCArray *bca, FrtBooleanClause *clause)
{
    if (clause) {
        bca_add_clause(bca, clause);
    }
    return bca;
}

/**
 * Add AND or OR clause to the BooleanClause array, depending on the default
 * clause type.
 */
static FrtBCArray *add_default_cls(FrtQParser *qp, FrtBCArray *bca,
                                FrtBooleanClause *clause)
{
    if (qp->or_default) {
        add_or_cls(bca, clause);
    }
    else {
        add_and_cls(bca, clause);
    }
    return bca;
}

/**
 * destroy array of BooleanClauses
 */
static void bca_destroy(FrtBCArray *bca)
{
    int i;
    for (i = 0; i < bca->size; i++) {
        frt_bc_deref(bca->clauses[i]);
    }
    free(bca->clauses);
    free(bca);
}

/**
 * Turn a query into a BooleanClause for addition to a BooleanQuery.
 */
static FrtBooleanClause *get_bool_cls(FrtQuery *q, FrtBCType occur)
{
    if (q) {
        return frt_bc_new(q, occur);
    }
    else {
        return NULL;
    }
}

/**
 * Create a TermQuery. The word will be tokenized and if the tokenization
 * produces more than one token, a PhraseQuery will be returned. For example,
 * if the word is dbalmain@gmail.com and a LetterTokenizer is used then a
 * PhraseQuery "dbalmain gmail com" will be returned which is actually exactly
 * what we want as it will match any documents containing the same email
 * address and tokenized with the same tokenizer.
 */
static FrtQuery *get_term_q(FrtQParser *qp, FrtSymbol field, char *word, rb_encoding *encoding)
{
    FrtQuery *q;
    FrtToken *token;
    FrtTokenStream *stream = get_cached_ts(qp, field, word, encoding);

    if ((token = frt_ts_next(stream)) == NULL) {
        q = NULL;
    }
    else {
        q = frt_tq_new(field, token->text);
        if ((token = frt_ts_next(stream)) != NULL) {
            /* Less likely case, destroy the term query and create a
             * phrase query instead */
            FrtQuery *phq = frt_phq_new(field);
            frt_phq_add_term(phq, ((FrtTermQuery *)q)->term, 0);
            q->destroy_i(q);
            q = phq;
            do {
                if (token->pos_inc) {
                    frt_phq_add_term(q, token->text, token->pos_inc);
                    /* add some slop since single term was expected */
                    ((FrtPhraseQuery *)q)->slop++;
                }
                else {
                    frt_phq_append_multi_term(q, token->text);
                }
            } while ((token = frt_ts_next(stream)) != NULL);
        }
    }
    return q;
}

/**
 * Create a FuzzyQuery. The word will be tokenized and only the first token
 * will be used. If there are any more tokens after tokenization, they will be
 * ignored.
 */
static FrtQuery *get_fuzzy_q(FrtQParser *qp, FrtSymbol field, char *word, char *slop_str, rb_encoding *encoding)
{
    FrtQuery *q;
    FrtToken *token;
    FrtTokenStream *stream = get_cached_ts(qp, field, word, encoding);

    if ((token = frt_ts_next(stream)) == NULL) {
        q = NULL;
    }
    else {
        /* it only makes sense to find one term in a fuzzy query */
        float slop = frt_qp_default_fuzzy_min_sim;
        if (slop_str) {
            sscanf(slop_str, "%f", &slop);
        }
        q = frt_fuzq_new_conf(field, token->text, slop, frt_qp_default_fuzzy_pre_len,
                          qp->max_clauses);
    }
    return q;
}

/**
 * Downcase a string taking encoding into account and works for multibyte character sets.
 */
static char *lower_str(char *str, int len, rb_encoding *enc) {
    OnigCaseFoldType fold_type = ONIGENC_CASE_DOWNCASE;
    const int max_len = len + 20; // CASE_MAPPING_ADDITIONAL_LENGTH
    char *buf = FRT_ALLOC_N(char, max_len);
    char *buf_end = buf + max_len + 19;
    const OnigUChar *t = (const OnigUChar *)str;

    len = enc->case_map(&fold_type, &t, (const OnigUChar *)(str + len), (OnigUChar *)buf, (OnigUChar *)buf_end, enc);
    memcpy(str, buf, len);
    str[len] = '\0';
    free(buf);

    return str;
}

/**
 * Create a WildCardQuery. No tokenization will be performed on the pattern
 * but the pattern will be downcased if +qp->wild_lower+ is set to true and
 * the field in question is a tokenized field.
 *
 * Note: this method will not always return a WildCardQuery. It could be
 * optimized to a MatchAllQuery if the pattern is '*' or a PrefixQuery if the
 * only wild char (*, ?) in the pattern is a '*' at the end of the pattern.
 */
static FrtQuery *get_wild_q(FrtQParser *qp, FrtSymbol field, char *pattern, rb_encoding *encoding) {
    FrtQuery *q;
    bool is_prefix = false;
    char *p;
    int len = (int)strlen(pattern);

    if (qp->wild_lower
        && (!qp->tokenized_fields || frt_hs_exists(qp->tokenized_fields, (void *)field))) {
        lower_str(pattern, len, encoding);
    }

    /* simplify the wildcard query to a prefix query if possible. Basically a
     * prefix query is any wildcard query that has a '*' as the last character
     * and no other wildcard characters before it. "*" by itself will expand
     * to a MatchAllQuery */
    if (strcmp(pattern, "*") == 0) {
        return frt_maq_new();
    }
    if (pattern[len - 1] == '*') {
        is_prefix = true;
        for (p = &pattern[len - 2]; p >= pattern; p--) {
            if (*p == '*' || *p == '?') {
                is_prefix = false;
                break;
            }
        }
    }
    if (is_prefix) {
        /* chop off the '*' temporarily to create the query */
        pattern[len - 1] = 0;
        q = frt_prefixq_new(field, pattern);
        pattern[len - 1] = '*';
    }
    else {
        q = frt_wcq_new(field, pattern);
    }
    FrtMTQMaxTerms(q) = qp->max_clauses;
    return q;
}

/**
 * Adds another field to the top of the FieldStack.
 */
static FrtHashSet *add_field(FrtQParser *qp, const char *field_name)
{
    FrtSymbol field = rb_intern(field_name);
    if (qp->allow_any_fields || frt_hs_exists(qp->all_fields, (void *)field)) {
        frt_hs_add(qp->fields, (void *)field);
    }
    return qp->fields;
}

/**
 * The method gets called when a field modifier ("field1|field2:") is seen. It
 * will push a new FieldStack object onto the stack and add +field+ to its
 * fields set.
 */
static FrtHashSet *first_field(FrtQParser *qp, const char *field_name)
{
    qp_push_fields(qp, frt_hs_new_ptr(NULL), true);
    return add_field(qp, field_name);
}

/**
 * Destroy a phrase object freeing all allocated memory.
 */
static void ph_destroy(Phrase *self)
{
    int i;
    for (i = 0; i < self->size; i++) {
        frt_ary_destroy(self->positions[i].terms, &free);
    }
    free(self->positions);
    free(self);
}


/**
 * Allocate a new Phrase object
 */
static Phrase *ph_new()
{
  Phrase *self = FRT_ALLOC_AND_ZERO(Phrase);
  self->capa = PHRASE_INIT_CAPA;
  self->positions = FRT_ALLOC_AND_ZERO_N(FrtPhrasePosition, PHRASE_INIT_CAPA);
  return self;
}

/**
 * Add the first word to the phrase. This method is also in charge of
 * allocating a new Phrase object.
 */
static Phrase *ph_first_word(char *word)
{
    Phrase *self = ph_new();
    if (word) { /* no point in adding NULL in start */
        self->positions[0].terms = frt_ary_new_type_capa(char *, 1);
        frt_ary_push(self->positions[0].terms, frt_estrdup(word));
        self->size = 1;
    }
    return self;
}

/**
 * Add a new word to the Phrase
 */
static Phrase *ph_add_word(Phrase *self, char *word)
{
    if (word) {
        const int index = self->size;
        FrtPhrasePosition *pp = self->positions;
        if (index >= self->capa) {
            self->capa <<= 1;
            FRT_REALLOC_N(pp, FrtPhrasePosition, self->capa);
            self->positions = pp;
        }
        pp[index].pos = self->pos_inc;
        pp[index].terms = frt_ary_new_type_capa(char *, 1);
        frt_ary_push(pp[index].terms, frt_estrdup(word));
        self->size++;
        self->pos_inc = 0;
    }
    else {
        self->pos_inc++;
    }
    return self;
}

/**
 * Adds a word to the Phrase object in the same position as the previous word
 * added to the Phrase. This will later be turned into a multi-PhraseQuery.
 */
static Phrase *ph_add_multi_word(Phrase *self, char *word)
{
    const int index = self->size - 1;
    FrtPhrasePosition *pp = self->positions;

    if (word) {
        frt_ary_push(pp[index].terms, frt_estrdup(word));
    }
    return self;
}

/**
 * Build a phrase query for a single field. It might seem like a better idea
 * to build the PhraseQuery once and duplicate it for each field but this
 * would be buggy in the case of PerFieldAnalyzers in which case a different
 * tokenizer could be used for each field.
 *
 * Note that the query object returned by this method is not always a
 * PhraseQuery. If there is only one term in the query then the query is
 * simplified to a TermQuery. If there are multiple terms but only a single
 * position, then a MultiTermQuery is retured.
 *
 * Note that each word in the query gets tokenized. Unlike get_term_q, if the
 * word gets tokenized into more than one token, the rest of the tokens are
 * ignored. For example, if you have the phrase;
 *
 *      "email: dbalmain@gmail.com"
 *
 * the Phrase object will contain two positions with the words 'email:' and
 * 'dbalmain@gmail.com'. Now, if you are using a LetterTokenizer then the
 * second word will be tokenized into the tokens ['dbalmain', 'gmail', 'com']
 * and only the first token will be used, so the resulting phrase query will
 * actually look like this;
 *
 *      "email dbalmain"
 *
 * This problem can easily be solved by using the StandardTokenizer or any
 * custom tokenizer which will leave dbalmain@gmail.com as a single token.
 */
static FrtQuery *get_phrase_query(FrtQParser *qp, FrtSymbol field, Phrase *phrase, char *slop_str, rb_encoding *encoding)
{
    const int pos_cnt = phrase->size;
    FrtQuery *q = NULL;

    if (pos_cnt == 1) {
        char **words = phrase->positions[0].terms;
        const int word_count = frt_ary_size(words);
        if (word_count == 1) {
            q = get_term_q(qp, field, words[0], encoding);
        }
        else {
            int i;
            int term_cnt = 0;
            FrtToken *token;
            char *last_word = NULL;

            for (i = 0; i < word_count; i++) {
                token = frt_ts_next(get_cached_ts(qp, field, words[i], encoding));
                if (token) {
                    free(words[i]);
                    last_word = words[i] = frt_estrdup(token->text);
                    ++term_cnt;
                }
                else {
                    /* empty words will later be ignored */
                    words[i][0] = '\0';
                }
            }

            switch (term_cnt) {
                case 0:
                    q = frt_bq_new(false);
                    break;
                case 1:
                    q = frt_tq_new(field, last_word);
                    break;
                default:
                    q = frt_multi_tq_new_conf(field, term_cnt, 0.0);
                    for (i = 0; i < word_count; i++) {
                        /* ignore empty words */
                        if (words[i][0]) {
                            frt_multi_tq_add_term(q, words[i]);
                        }
                    }
                    break;
            }
        }
    }
    else if (pos_cnt > 1) {
        FrtToken *token;
        FrtTokenStream *stream;
        int i, j;
        int pos_inc = 0;
        q = frt_phq_new(field);
        if (slop_str) {
            int slop;
            sscanf(slop_str,"%d",&slop);
            ((FrtPhraseQuery *)q)->slop = slop;
        }

        for (i = 0; i < pos_cnt; i++) {
            char **words = phrase->positions[i].terms;
            const int word_count = frt_ary_size(words);
            if (pos_inc) {
                ((FrtPhraseQuery *)q)->slop++;
            }
            pos_inc += phrase->positions[i].pos + 1; /* Actually holds pos_inc*/

            if (word_count == 1) {
                stream = get_cached_ts(qp, field, words[0], encoding);
                while ((token = frt_ts_next(stream))) {
                    if (token->pos_inc) {
                        frt_phq_add_term(q, token->text,
                                     pos_inc ? pos_inc : token->pos_inc);
                    }
                    else {
                        frt_phq_append_multi_term(q, token->text);
                        ((FrtPhraseQuery *)q)->slop++;
                    }
                    pos_inc = 0;
                }
            }
            else {
                bool added_position = false;

                for (j = 0; j < word_count; j++) {
                    stream = get_cached_ts(qp, field, words[j], encoding);
                    if ((token = frt_ts_next(stream))) {
                        if (!added_position) {
                            frt_phq_add_term(q, token->text,
                                         pos_inc ? pos_inc : token->pos_inc);
                            added_position = true;
                            pos_inc = 0;
                        }
                        else {
                            frt_phq_append_multi_term(q, token->text);
                        }
                    }
                }
            }
        }
    }
    return q;
}

/**
 * Get a phrase query from the Phrase object. The Phrase object is built up by
 * the query parser as the all PhraseQuery didn't work well for this. Once the
 * PhraseQuery has been built the Phrase object needs to be destroyed.
 */
static FrtQuery *get_phrase_q(FrtQParser *qp, Phrase *phrase, char *slop_str, rb_encoding *encoding)
{
    FrtQuery *volatile q = NULL;
    FLDS(q, get_phrase_query(qp, field, phrase, slop_str, encoding));
    ph_destroy(phrase);
    return q;
}

/**
 * Gets a RangeQuery object.
 *
 * Just like with WildCardQuery, RangeQuery needs to downcase its terms if the
 * tokenizer also downcased its terms.
 */
static FrtQuery *get_r_q(FrtQParser *qp, FrtSymbol field, char *from, char *to, bool inc_lower, bool inc_upper, rb_encoding *encoding) {
    FrtQuery *rq;
    if (qp->wild_lower
        && (!qp->tokenized_fields || frt_hs_exists(qp->tokenized_fields, (void *)field))) {
        if (from)
            lower_str(from, strlen(from), encoding);
        if (to)
            lower_str(to, strlen(to), encoding);
    }
/*
 * terms don't get tokenized as it doesn't really make sense to do so for
 * range queries.

    if (from) {
        FrtTokenStream *stream = get_cached_ts(qp, field, from, encoding);
        FrtToken *token = frt_ts_next(stream);
        from = token ? frt_estrdup(token->text) : NULL;
    }
    if (to) {
        FrtTokenStream *stream = get_cached_ts(qp, field, to, encoding);
        FrtToken *token = frt_ts_next(stream);
        to = token ? frt_estrdup(token->text) : NULL;
    }
*/

    rq = qp->use_typed_range_query ?
        frt_trq_new(field, from, to, inc_lower, inc_upper) :
        frt_rq_new(field, from, to, inc_lower, inc_upper);
    return rq;
}

/**
 * Every time the query parser sees a new field modifier ("field1|field2:")
 * it pushes a new FieldStack object onto the stack and sets its fields to the
 * fields specified in the fields modifier. If the field modifier is '*',
 * fs->fields is set to all_fields. fs->fields is set to +qp->def_field+ at
 * the bottom of the stack (ie the very first set of fields pushed onto the
 * stack).
 */
static void qp_push_fields(FrtQParser *self, FrtHashSet *fields, bool destroy)
{
    FrtFieldStack *fs = FRT_ALLOC(FrtFieldStack);

    fs->next    = self->fields_top;
    fs->fields  = fields;
    fs->destroy = destroy;

    self->fields_top = fs;
    self->fields = fields;
}

/**
 * Pops the top of the fields stack and frees any memory used by it. This will
 * get called when query modified by a field modifier ("field1|field2:") has
 * been fully parsed and the field specifier no longer applies.
 */
static void qp_pop_fields(FrtQParser *self)
{
    FrtFieldStack *fs = self->fields_top;

    if (fs->destroy) {
        frt_hs_destroy(fs->fields);
    }
    self->fields_top = fs->next;
    if (self->fields_top) {
        self->fields = self->fields_top->fields;
    }
    free(fs);
}

/**
 * Free all memory allocated by the QueryParser.
 */
void frt_qp_destroy(FrtQParser *self)
{
    if (self->tokenized_fields != self->all_fields) {
        frt_hs_destroy(self->tokenized_fields);
    }
    if (self->def_fields != self->all_fields) {
        frt_hs_destroy(self->def_fields);
    }
    frt_hs_destroy(self->all_fields);

    qp_pop_fields(self);
    assert(NULL == self->fields_top);

    frt_h_destroy(self->ts_cache);
    frt_ts_deref(self->non_tokenizer);
    frt_a_deref(self->analyzer);
    free(self);
}

FrtQParser *frt_qp_alloc() {
    return FRT_ALLOC(FrtQParser);
}

FrtQParser *frt_qp_init(FrtQParser *self, FrtAnalyzer *analyzer) {
    self->or_default = true;
    self->wild_lower = true;
    self->clean_str = false;
    self->max_clauses = FRT_QP_MAX_CLAUSES;
    self->handle_parse_errors = false;
    self->allow_any_fields = false;
    self->use_keywords = true;
    self->use_typed_range_query = false;
    self->def_slop = 0;

    self->def_fields = frt_hs_new_ptr(NULL);
    self->all_fields = frt_hs_new_ptr(NULL);
    self->tokenized_fields = frt_hs_new_ptr(NULL);
    self->fields_top = NULL;

    /* make sure all_fields contains the default fields */
    qp_push_fields(self, self->def_fields, false);

    self->analyzer = analyzer;
    self->ts_cache = frt_h_new_ptr((frt_free_ft)&frt_ts_deref);
    self->buf_index = 0;
    self->dynbuf = NULL;
    self->non_tokenizer = frt_non_tokenizer_new();
    frt_mutex_init(&self->mutex, NULL);
    return self;
}

/**
 * Creates a new QueryParser setting all boolean parameters to their defaults.
 * If +def_fields+ is NULL then +all_fields+ is used in place of +def_fields+.
 * Not also that this method ensures that all fields that exist in
 * +def_fields+ must also exist in +all_fields+. This should make sense.
 */
FrtQParser *frt_qp_new(FrtAnalyzer *analyzer) {
    FrtQParser *self = frt_qp_alloc();
    return frt_qp_init(self, analyzer);
}

void frt_qp_add_field(FrtQParser *self, FrtSymbol field, bool is_default, bool is_tokenized)
{
    frt_hs_add(self->all_fields, (void *)field);
    if (is_default) {
        frt_hs_add(self->def_fields, (void *)field);
    }
    if (is_tokenized) {
        frt_hs_add(self->tokenized_fields, (void *)field);
    }
}

/* these chars have meaning within phrases */
static const char *PHRASE_CHARS = "<>|\"";

/**
 * +str_insert_char+ inserts a character at the beginning of a string by
 * shifting the rest of the string right.
 */
static void str_insert_char(char *str, int len, char chr)
{
    memmove(str+1, str, len*sizeof(char));
    *str = chr;
}

/**
 * +frt_qp_clean_str+ basically scans the query string and ensures that all open
 * and close parentheses '()' and quotes '"' are balanced. It does this by
 * inserting or appending extra parentheses or quotes to the string. This
 * obviously won't necessarily be exactly what the user wanted but we are
 * never going to know that anyway. The main job of this method is to help the
 * query at least parse correctly.
 *
 * It also checks that all special characters within phrases (ie between
 * quotes) are escaped correctly unless they have meaning within a phrase
 * ( <>,|," ). Note that '<' and '>' will also be escaped unless the appear
 * together like so; '<>'.
 */
char *frt_qp_clean_str(char *str)
{
    int b, pb = -1;
    int br_cnt = 0;
    bool quote_open = false;
    char *sp, *nsp;

    /* leave a little extra */
    char *new_str = FRT_ALLOC_N(char, strlen(str)*2 + 1);

    for (sp = str, nsp = new_str; *sp; sp++) {
        b = *sp;
        /* ignore escaped characters */
        if (pb == '\\') {
            if (quote_open && strrchr(PHRASE_CHARS, b)) {
                *nsp++ = '\\'; /* this was left off the first time through */
            }
            *nsp++ = b;
            /* \ has escaped itself so has no power. Assign pb random char 'r' */
            pb = ((b == '\\') ? 'r' : b);
            continue;
        }
        switch (b) {
            case '\\':
                if (!quote_open) { /* We do our own escaping below */
                    *nsp++ = b;
                }
                break;
            case '"':
                quote_open = !quote_open;
                *nsp++ = b;
                break;
            case '(':
              if (!quote_open) {
                  br_cnt++;
              }
              else {
                  *nsp++ = '\\';
              }
              *nsp++ = b;
              break;
            case ')':
                if (!quote_open) {
                    if (br_cnt == 0) {
                        str_insert_char(new_str, (int)(nsp - new_str), '(');
                        nsp++;
                    }
                    else {
                        br_cnt--;
                    }
                }
                else {
                    *nsp++ = '\\';
                }
                *nsp++ = b;
                break;
            case '>':
                if (quote_open) {
                    if (pb == '<') {
                        /* remove the escape character */
                        nsp--;
                        nsp[-1] = '<';
                    }
                    else {
                        *nsp++ = '\\';
                    }
                }
                *nsp++ = b;
                break;
            default:
                if (quote_open) {
                    if (strrchr(special_char, b) && b != '|') {
                        *nsp++ = '\\';
                    }
                }
                *nsp++ = b;
        }
        pb = b;
    }
    if (quote_open) {
        *nsp++ = '"';
    }
    for (;br_cnt > 0; br_cnt--) {
      *nsp++ = ')';
    }
    *nsp = '\0';
    return new_str;
}

/**
 * Takes a string and finds whatever tokens it can using the QueryParser's
 * analyzer. It then turns these tokens (if any) into a boolean query. If it
 * fails to find any tokens, this method will return NULL.
 */
static FrtQuery *qp_get_bad_query(FrtQParser *qp, char *str, rb_encoding *encoding)
{
    FrtQuery *volatile q = NULL;
    qp->recovering = true;
    assert(qp->fields_top->next == NULL);
    FLDS(q, get_term_q(qp, field, str, encoding));
    return q;
}

/**
 * +qp_parse+ takes a string and turns it into a Query object using Ferret's
 * query language. It must either raise an error or return a query object. It
 * must not return NULL. If the yacc parser fails it will use a very basic
 * boolean query parser which takes whatever tokens it can find in the query
 * and turns them into a boolean query on the default fields.
 */

FrtQuery *qp_parse(FrtQParser *self, char *query_string, rb_encoding *encoding)
{
    FrtQuery *result = NULL;
    char *qstr;
    unsigned char *dp_start = NULL;

    frt_mutex_lock(&self->mutex);
    /* if qp->fields_top->next is not NULL we have a left over field-stack
     * object that was not popped during the last query parse */
    assert(NULL == self->fields_top->next);

    /* encode query_string to utf8 for futher processing unless it is utf8 encoded */
    if (encoding == utf8_encoding) {
        qstr = query_string;
    } else {
        /* assume query is sbc encoded und encoding to utf results in maximum utf mbc expansion */
        const unsigned char *sp = (unsigned char *)query_string;
        int query_string_len = strlen(query_string);
        int dp_length = query_string_len * utf8_mbmaxlen + 1;
        unsigned char *dp = FRT_ALLOC_N(unsigned char, dp_length);
        dp_start = dp;
        rb_econv_t *ec = rb_econv_open(rb_enc_name(encoding), rb_enc_name(utf8_encoding), RUBY_ECONV_INVALID_REPLACE);
        assert(ec != NULL);
        rb_econv_convert(ec, &sp, (unsigned char *)query_string + query_string_len, &dp, (unsigned char *)dp + dp_length - 1, 0);
        rb_econv_close(ec);
        *dp = '\0';
        qstr = (char *)dp_start;
    }

    self->recovering = self->destruct = false;

    if (self->clean_str) {
        self->qstrp = self->qstr = frt_qp_clean_str(qstr);
    } else {
        self->qstrp = self->qstr = qstr;
    }
    self->fields = self->def_fields;
    self->result = NULL;

    if (0 == yyparse(self, encoding))
      result = self->result;

    if (!result && self->handle_parse_errors) {
        self->destruct = false;
        result = qp_get_bad_query(self, self->qstr, encoding);
    }
    if (self->destruct && !self->handle_parse_errors)
        FRT_RAISE(FRT_PARSE_ERROR, frt_xmsg_buffer);

    if (!result)
        result = frt_bq_new(false);

    if (self->clean_str)
        free(self->qstr);
    if (dp_start)
        free(dp_start);

    frt_mutex_unlock(&self->mutex);
    return result;
}
