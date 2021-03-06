#include "frt_search.h"
#include "isomorfeus_ferret.h"

static VALUE cQueryParser;
static VALUE cQueryParseException;

extern VALUE sym_analyzer;
static VALUE sym_wild_card_downcase;
static VALUE sym_fields;
static VALUE sym_all_fields;
static VALUE sym_tkz_fields;
static VALUE sym_default_field;
static VALUE sym_validate_fields;
static VALUE sym_or_default;
static VALUE sym_default_slop;
static VALUE sym_handle_parse_errors;
static VALUE sym_clean_string;
static VALUE sym_max_clauses;
static VALUE sym_use_keywords;
static VALUE sym_use_typed_range_query;

extern VALUE frb_get_analyzer(FrtAnalyzer *a);
extern VALUE frb_get_q(FrtQuery *q);
extern FrtAnalyzer *frb_get_cwrapped_analyzer(VALUE ranalyzer);

/****************************************************************************
 *
 * QueryParser Methods
 *
 ****************************************************************************/

static void frb_qp_free(void *p) {
    frt_qp_destroy((FrtQParser *)p);
}

static void frb_qp_mark(void *p) {
    if (((FrtQParser *)p)->analyzer && ((FrtQParser *)p)->analyzer->ranalyzer)
        rb_gc_mark(((FrtQParser *)p)->analyzer->ranalyzer);
}

static size_t frb_qp_size(const void *p) {
    return sizeof(FrtQParser);
    (void)p;
}

const rb_data_type_t frb_qp_t = {
    .wrap_struct_name = "FrbQueryParser",
    .function = {
        .dmark = frb_qp_mark,
        .dfree = frb_qp_free,
        .dsize = frb_qp_size,
        .dcompact = NULL,
        .reserved = {0},
    },
    .parent = NULL,
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE frb_qp_alloc(VALUE rclass) {
    FrtQParser *qp = frt_qp_alloc();
    qp->analyzer = NULL;
    return TypedData_Wrap_Struct(rclass, &frb_qp_t, qp);
}

static FrtHashSet *frb_get_fields(VALUE rfields, FrtHashSet *other_fields) {
    VALUE rval;
    FrtHashSet *fields;
    char *s, *p, *str;

    if (rfields == Qnil) return NULL;

    fields = frt_hs_new_ptr(NULL);
    if (TYPE(rfields) == T_ARRAY) {
        int i;
        for (i = 0; i < RARRAY_LEN(rfields); i++) {
            rval = rb_obj_as_string(RARRAY_PTR(rfields)[i]);
            frt_hs_add(fields, (void *)rb_intern(rs2s(rval)));
        }
    } else {
        rval = rb_obj_as_string(rfields);
        if (strcmp("*", rs2s(rval)) == 0) {
            frt_hs_destroy(fields);
            fields = NULL;
        } else {
            s = str = rstrdup(rval);
            while ((p = strchr(s, '|')) && *p != '\0') {
                *p = '\0';
                frt_hs_add(fields, (void *)rb_intern(s));
                s = p + 1;
            }
            frt_hs_add(fields, (void *)rb_intern(s));
            free(str);
        }
    }
    return fields;
}

static void hs_safe_merge(FrtHashSet *merger, FrtHashSet *mergee) {
    FrtHashSetEntry *entry = mergee->first;
    for (; entry != NULL; entry = entry->next) {
        frt_hs_add_safe(merger, entry->elem);
    }
}

/*
 *  call-seq:
 *     QueryParser.new(options = {}) -> QueryParser
 *
 *  Create a new QueryParser. The QueryParser is used to convert string
 *  queries into Query objects. The options are;
 *
 *  === Options
 *
 *  :default_field::         Default: "*" (all fields). The default field to
 *                           search when no field is specified in the search
 *                           string. It can also be an array of fields.
 *  :analyzer::              Default: StandardAnalyzer. FrtAnalyzer used by the
 *                           query parser to parse query terms
 *  :wild_card_downcase::    Default: true. Specifies whether wild-card queries
 *                           and range queries should be downcased or not since
 *                           they are not passed through the parser
 *  :fields::                Default: []. Lets the query parser know what
 *                           fields are available for searching, particularly
 *                           when the "*" is specified as the search field
 *  :tokenized_fields::      Default: :fields. Lets the query parser know which
 *                           fields are tokenized so it knows which fields to
 *                           run the analyzer over.
 *  :validate_fields::       Default: false. Set to true if you want an
 *                           exception to be raised if there is an attempt to
 *                           search a non-existent field
 *  :or_default::            Default: true. Use "OR" as the default boolean
 *                           operator
 *  :default_slop::          Default: 0. Default slop to use in PhraseQuery
 *  :handle_parse_errors::   Default: true. QueryParser will quietly handle all
 *                           parsing errors internally. If you'd like to handle
 *                           them yourself, set this parameter to false.
 *  :clean_string::          Default: true. QueryParser will do a quick
 *                           once-over the query string make sure that quotes
 *                           and brackets match up and special characters are
 *                           escaped
 *  :max_clauses::           Default: 512. the maximum number of clauses
 *                           allowed in boolean queries and the maximum number
 *                           of terms allowed in multi, prefix, wild-card or
 *                           fuzzy queries when those queries are generated by
 *                           rewriting other queries
 *  :use_keywords::          Default: true. By default AND, OR, NOT and REQ are
 *                           keywords used by the query parser. Sometimes this
 *                           is undesirable. For example, if your application
 *                           allows searching for US states by their
 *                           abbreviation, then OR will be a common query
 *                           string. By setting :use_keywords to false, OR will
 *                           no longer be a keyword allowing searches for the
 *                           state of Oregon. You will still be able to use
 *                           boolean queries by using the + and - characters.
 *  :use_typed_range_query:: Default: false. Use TypedRangeQuery instead of
 *                           the standard RangeQuery when parsing
 *                           range queries. This is useful if you have number
 *                           fields which you want to perform range queries
 *                           on. You won't need to pad or normalize the data
 *                           in the field in anyway to get correct results.
 *                           However, performance will be a lot slower for
 *                           large indexes, hence the default.
 *                           Note: the default is set to true in the Index
 *                           class.
 */
static VALUE frb_qp_init(int argc, VALUE *argv, VALUE self) {
    VALUE roptions = Qnil;
    VALUE rval;
    FrtAnalyzer *analyzer = NULL;
    FrtHashSet *def_fields = NULL;
    FrtHashSet *all_fields = NULL;
    FrtHashSet *tkz_fields = NULL;
    FrtQParser *qp;
    TypedData_Get_Struct(self, FrtQParser, &frb_qp_t, qp);
    if (rb_scan_args(argc, argv, "01", &roptions) > 0) {
        if (TYPE(roptions) == T_HASH) {
            if (Qnil != (rval = rb_hash_aref(roptions, sym_default_field))) {
                def_fields = frb_get_fields(rval, NULL);
            }
            if (Qnil != (rval = rb_hash_aref(roptions, sym_analyzer))) {
                analyzer = frb_get_cwrapped_analyzer(rval);
            }
            if (Qnil != (rval = rb_hash_aref(roptions, sym_all_fields))) {
                all_fields = frb_get_fields(rval, def_fields);
            }
            if (Qnil != (rval = rb_hash_aref(roptions, sym_fields))) {
                all_fields = frb_get_fields(rval, def_fields);
            }
            if (Qnil != (rval = rb_hash_aref(roptions, sym_tkz_fields))) {
                tkz_fields = frb_get_fields(rval, def_fields);
            }
        } else {
            def_fields = frb_get_fields(roptions, def_fields);
            roptions = Qnil;
        }
    }
    if (all_fields == NULL) {
        all_fields = frt_hs_new_ptr(NULL);
    }
    if (!analyzer) {
        analyzer = frt_standard_analyzer_new(true);
    }
    qp = frt_qp_init(qp, analyzer);
    if (def_fields) hs_safe_merge(all_fields, def_fields);
    if (tkz_fields) hs_safe_merge(all_fields, tkz_fields);
    qp->all_fields = all_fields;
    qp->def_fields = def_fields ? def_fields : all_fields;
    qp->tokenized_fields = tkz_fields ? tkz_fields : all_fields;
    qp->fields_top->fields = def_fields;

    qp->allow_any_fields = true;
    qp->clean_str = true;
    qp->handle_parse_errors = true;
    /* handle options */
    if (roptions != Qnil) {
        if (Qnil != (rval = rb_hash_aref(roptions, sym_handle_parse_errors))) {
            qp->handle_parse_errors = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_validate_fields))) {
            qp->allow_any_fields = !RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_wild_card_downcase))) {
            qp->wild_lower = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_or_default))) {
            qp->or_default = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_default_slop))) {
            qp->def_slop = FIX2INT(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_clean_string))) {
            qp->clean_str = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_max_clauses))) {
            qp->max_clauses = FIX2INT(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_use_keywords))) {
            qp->use_keywords = RTEST(rval);
        }
        if (Qnil != (rval = rb_hash_aref(roptions, sym_use_typed_range_query))){
            qp->use_typed_range_query = RTEST(rval);
        }
    }
    return self;
}

#define GET_QP FrtQParser *qp = (FrtQParser *)DATA_PTR(self)
/*
 *  call-seq:
 *     query_parser.parse(query_string) -> Query
 *
 *  Parse a query string returning a Query object if parsing was successful.
 *  Will raise a QueryParseException if unsuccessful.
 */
static VALUE frb_qp_parse(VALUE self, VALUE rstr) {
    const char *volatile msg = NULL;
    volatile VALUE rq;
    GET_QP;
    FrtQuery *q;
    rstr = rb_obj_as_string(rstr);
    FRT_TRY
        q = qp_parse(qp, rs2s(rstr), rb_enc_get(rstr));
        rq = frb_get_q(q);
        break;
    FRT_XCATCHALL
        msg = xcontext.msg;
        FRT_HANDLED();
    FRT_XENDTRY
    if (msg) {
        rb_raise(cQueryParseException, "%s", msg);
    }
    return rq;
}

/*
 *  call-seq:
 *     query_parser.fields -> Array of Symbols
 *
 *  Returns the list of all fields that the QueryParser knows about.
 */
static VALUE frb_qp_get_fields(VALUE self) {
    GET_QP;
    FrtHashSet *fields = qp->all_fields;
    FrtHashSetEntry *hse;
    VALUE rfields = rb_ary_new();

    for (hse = fields->first; hse; hse = hse->next) {
        rb_ary_push(rfields, ID2SYM(rb_intern((char *)hse->elem)));
    }

    return rfields;
}

/*
 *  call-seq:
 *     query_parser.fields = fields -> self
 *
 *  Set the list of fields. These fields are expanded for searches on "*".
 */
static VALUE frb_qp_set_fields(VALUE self, VALUE rfields) {
    GET_QP;
    FrtHashSet *fields = frb_get_fields(rfields, NULL);

    /* if def_fields == all_fields then we need to replace both */
    if (qp->def_fields == qp->all_fields) qp->def_fields = NULL;
    if (qp->tokenized_fields == qp->all_fields) qp->tokenized_fields = NULL;

    if (fields == NULL) {
        fields = frt_hs_new_ptr(NULL);
    }

    /* make sure all the fields in tokenized fields are contained in
     * all_fields */
    if (qp->tokenized_fields) hs_safe_merge(fields, qp->tokenized_fields);

    /* delete old fields set */
    assert(qp->all_fields->free_elem_i == frt_dummy_free);
    frt_hs_destroy(qp->all_fields);

    /* add the new fields set and add to def_fields if necessary */
    qp->all_fields = fields;
    if (qp->def_fields == NULL) {
        qp->def_fields = fields;
        qp->fields_top->fields = fields;
    }
    if (qp->tokenized_fields == NULL) qp->tokenized_fields = fields;

    return self;
}

/*
 *  call-seq:
 *     query_parser.tokenized_fields -> Array of Symbols
 *
 *  Returns the list of all tokenized_fields that the QueryParser knows about.
 */
static VALUE frb_qp_get_tkz_fields(VALUE self) {
    GET_QP;
    FrtHashSet *fields = qp->tokenized_fields;
    if (fields) {
        VALUE rfields = rb_ary_new();
        FrtHashSetEntry *hse;

        for (hse = fields->first; hse; hse = hse->next) {
            rb_ary_push(rfields, ID2SYM(rb_intern((char *)hse->elem)));
        }

        return rfields;
    } else {
        return Qnil;
    }
}

/*
 *  call-seq:
 *     query_parser.tokenized_fields = fields -> self
 *
 *  Set the list of tokenized_fields. These tokenized_fields are tokenized in
 *  the queries. If this is set to Qnil then all fields will be tokenized.
 */
static VALUE frb_qp_set_tkz_fields(VALUE self, VALUE rfields) {
    GET_QP;
    if (qp->tokenized_fields != qp->all_fields) {
        frt_hs_destroy(qp->tokenized_fields);
    }
    qp->tokenized_fields = frb_get_fields(rfields, NULL);
    return self;
}

/****************************************************************************
 *
 * Init function
 *
 ****************************************************************************/

/* rdoc hack
extern VALUE mFerret = rb_define_module("Ferret");
extern VALUE cQueryParser = rb_define_module_under(mFerret, "QueryParser");
*/

/*
 *  Document-class: Ferret::QueryParser::QueryParseException
 *
 *  == Summary
 *
 *  Exception raised when there is an error parsing the query string passed to
 *  QueryParser.
 */
void Init_QueryParseException(void) {
    cQueryParseException = rb_define_class_under(cQueryParser, "QueryParseException", rb_eStandardError);
}

/*
 *  Document-class: Ferret::QueryParser
 *
 *  == Summary
 *
 *  The QueryParser is used to transform user submitted query strings into
 *  QueryObjects. Ferret using its own Query Language known from now on as
 *  Ferret Query Language or FQL.
 *
 *  == Ferret Query Language
 *
 *  === Preamble
 *
 *  The following characters are special characters in FQL;
 *
 *    :, (, ), [, ], {, }, !, +, ", ~, ^, -, |, <, >, =, *, ?, \
 *
 *  If you want to use one of these characters in one of your terms you need
 *  to escape it with a \ character. \ escapes itself. The exception to this
 *  rule is within Phrases which a strings surrounded by double quotes (and
 *  will be explained further bellow in the section on PhraseQueries). In
 *  Phrases, only ", | and <> have special meaning and need to be escaped if
 *  you want the literal value. <> is escaped \<\>.
 *
 *  In the following examples I have only written the query string. This would
 *  be parse like;
 *
 *    query = query_parser.parse("pet:(dog AND cat)")
 *    puts query    # => "+pet:dog +pet:cat"
 *
 *  === TermQuery
 *
 *  A term query is the most basic query of all and is what most of the other
 *  queries are built upon. The term consists of a single word. eg;
 *
 *    'term'
 *
 *  Note that the analyzer will be run on the term and if it splits the term
 *  in two then it will be turned into a phrase query. For example, with the
 *  plain Ferret::Analysis::Analyzer, the following;
 *
 *    'dave12balmain'
 *
 *  is equivalent to;
 *
 *    '"dave balmain"'
 *
 *  Which we will explain now...
 *
 *  === PhraseQuery
 *
 *  A phrase query is a string of terms surrounded by double quotes. For
 *  example you could write;
 *
 *    '"quick brown fox"'
 *
 *  But if a "fast" fox is just as good as a quick one you could use the |
 *  character to specify alternate terms.
 *
 *    '"quick|speedy|fast brown fox"'
 *
 *  What if we don't care what colour the fox is. We can use the <> to specify
 *  a place setter. eg;
 *
 *    '"quick|speedy|fast <> fox"'
 *
 *  This will match any word in between quick and fox. Alternatively we could
 *  set the "slop" for the phrase which allows a certain variation in the
 *  match of the phrase. The slop for a phrase is an integer indicating how
 *  many positions you are allowed to move the terms to get a match. Read more
 *  about the slop factor in Ferret::Search::PhraseQuery. To set the slop
 *  factor for a phrase you can type;
 *
 *    '"big house"~2'
 *
 *  This would match "big house", "big red house", "big red brick house" and
 *  even "house big". That's right, you don't need to have th terms in order
 *  if you allow some slop in your phrases. (See Ferret::Search::Spans if you
 *  need a phrase type query with ordered terms.)
 *
 *  These basic queries will be run on the default field which is set when you
 *  create the query_parser. But what if you want to search a different field.
 *  You'll be needing a ...
 *
 *  === FieldQuery
 *
 *  A field query is any field prefixed by <fieldname>:. For example, to
 *  search for all instances of the term "ski" in field "sport", you'd write;
 *
 *    'sport:ski'
 *  Or we can apply a field to phrase;
 *
 *    'sport:"skiing is fun"'
 *
 *  Now we have a few types of queries, we'll be needing to glue them together
 *  with a ...
 *
 *  === BooleanQuery
 *
 *  There are a couple of ways of writing boolean queries. Firstly you can
 *  specify which terms are required, optional or required not to exist (not).
 *
 *  * '+' or "REQ" can be used to indicate a required query. "REQ" must be
 *    surrounded by white space.
 *  * '-', '!' or "NOT" are used to indicate query that is required to be
 *    false. "NOT" must be surrounded by white space.
 *  * all other queries are optional if the above symbols are used.
 *
 *  Some examples;
 *
 *    '+sport:ski -sport:snowboard sport:toboggan'
 *    '+ingredient:chocolate +ingredient:strawberries -ingredient:wheat'
 *
 *  You may also use the boolean operators "AND", "&&", "OR" and "||". eg;
 *
 *    'sport:ski AND NOT sport:snowboard OR sport:toboggan'
 *    'ingredient:chocolate AND ingredient:strawberries AND NOT ingredient:wheat'
 *
 *  You can set the default operator when you create the query parse.
 *
 *  === RangeQuery
 *
 *  A range query finds all documents with terms between the two query terms.
 *  This can be very useful in particular for dates. eg;
 *
 *    'date:[20050725 20050905]' # all dates >= 20050725 and <= 20050905
 *    'date:[20050725 20050905}' # all dates >= 20050725 and <  20050905
 *    'date:{20050725 20050905]' # all dates >  20050725 and <= 20050905
 *    'date:{20050725 20050905}' # all dates >  20050725 and <  20050905
 *
 *  You can also do open ended queries like this;
 *
 *    'date:[20050725>' # all dates >= 20050725
 *    'date:{20050725>' # all dates >  20050725
 *    'date:<20050905]' # all dates <= 20050905
 *    'date:<20050905}' # all dates <  20050905
 *
 *  Or like this;
 *
 *    'date: >= 20050725'
 *    'date: >  20050725'
 *    'date: <= 20050905'
 *    'date: <  20050905'
 *
 *  If you prefer the above style you could use a boolean query but like this;
 *
 *    'date:( >= 20050725 AND <= 20050905)'
 *
 *  But rangequery only solution shown first will be faster.
 *
 *  === WildQuery
 *
 *  A wild query is a query using the pattern matching characters * and ?. *
 *  matches 0 or more characters while ? matches a single character. This type
 *  of query can be really useful for matching hierarchical categories for
 *  example. Let's say we had this structure;
 *
 *    /sport/skiing
 *    /sport/cycling
 *    /coding1/ruby
 *    /coding1/c
 *    /coding2/python
 *    /coding2/perl
 *
 *  If you wanted all categories with programming languages you could use the
 *  query;
 *
 *    'category:/coding?/?*'
 *
 *  Note that this query can be quite expensive if not used carefully. In the
 *  example above there would be no problem but you should be careful not use
 *  the wild characters at the beginning of the query as it'll have to iterate
 *  through every term in that field. Having said that, some fields like the
 *  category field above will only have a small number of distinct fields so
 *  this could be okay.
 *
 *  === FuzzyQuery
 *
 *  This is like the sloppy phrase query above, except you are now adding slop
 *  to a term. Basically it measures the Levenshtein distance between two
 *  terms and if the value is below the slop threshold the term is a match.
 *  This time though the slop must be a float between 0 and 1.0, 1.0 being a
 *  perfect match and 0 being far from a match. The default is set to 0.5 so
 *  you don't need to give a slop value if you don't want to. You can set the
 *  default in the Ferret::Search::FuzzyQuery class. Here are a couple of
 *  examples;
 *
 *    'content:ferret~'
 *    'content:Ostralya~0.4'
 *
 *  Note that this query can be quite expensive. If you'd like to use this
 *  query, you may want to set a minimum prefix length in the FuzzyQuery
 *  class. This can substantially reduce the number of terms that the query
 *  will iterate over.
 *
 */
void Init_QueryParser(void) {
    /* hash keys */
    sym_wild_card_downcase = ID2SYM(rb_intern("wild_card_downcase"));
    sym_fields = ID2SYM(rb_intern("fields"));
    sym_all_fields = ID2SYM(rb_intern("all_fields"));
    sym_tkz_fields = ID2SYM(rb_intern("tokenized_fields"));
    sym_default_field = ID2SYM(rb_intern("default_field"));
    sym_validate_fields = ID2SYM(rb_intern("validate_fields"));
    sym_or_default = ID2SYM(rb_intern("or_default"));
    sym_default_slop = ID2SYM(rb_intern("default_slop"));
    sym_handle_parse_errors = ID2SYM(rb_intern("handle_parse_errors"));
    sym_clean_string = ID2SYM(rb_intern("clean_string"));
    sym_max_clauses = ID2SYM(rb_intern("max_clauses"));
    sym_use_keywords = ID2SYM(rb_intern("use_keywords"));
    sym_use_typed_range_query = ID2SYM(rb_intern("use_typed_range_query"));

    /* QueryParser */
    cQueryParser = rb_define_class_under(mFerret, "QueryParser", rb_cObject);
    rb_define_alloc_func(cQueryParser, frb_qp_alloc);

    rb_define_method(cQueryParser, "initialize", frb_qp_init, -1);
    rb_define_method(cQueryParser, "parse", frb_qp_parse, 1);
    rb_define_method(cQueryParser, "fields", frb_qp_get_fields, 0);
    rb_define_method(cQueryParser, "fields=", frb_qp_set_fields, 1);
    rb_define_method(cQueryParser, "tokenized_fields", frb_qp_get_tkz_fields, 0);
    rb_define_method(cQueryParser, "tokenized_fields=", frb_qp_set_tkz_fields, 1);

    Init_QueryParseException();
}
