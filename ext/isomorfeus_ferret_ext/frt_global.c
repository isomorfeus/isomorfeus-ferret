#include "frt_global.h"
#include "frt_hash.h"
#include "frt_search.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ruby/encoding.h>

const char *FRT_EMPTY_STRING = "";

rb_encoding *utf8_encoding;
int utf8_mbmaxlen;
OnigCodePoint cp_apostrophe;
OnigCodePoint cp_dot;
OnigCodePoint cp_comma;
OnigCodePoint cp_backslash;
OnigCodePoint cp_slash;
OnigCodePoint cp_underscore;
OnigCodePoint cp_dash;
OnigCodePoint cp_hyphen;
OnigCodePoint cp_at;
OnigCodePoint cp_ampersand;
OnigCodePoint cp_colon;

int frt_scmp(const void *p1, const void *p2) {
    return strcmp(*(char **) p1, *(char **) p2);
}

void frt_strsort(char **str_array, int size) {
    qsort(str_array, size, sizeof(char *), &frt_scmp);
}

int frt_icmp(const void *p1, const void *p2) {
    int i1 = *(int *) p1;
    int i2 = *(int *) p2;

    if (i1 > i2) {
        return 1;
    }
    else if (i1 < i2) {
        return -1;
    }
    return 0;
}

int frt_icmp_risky(const void *p1, const void *p2) {
  return (*(int *)p1) - *((int *)p2);
}

unsigned int *frt_imalloc(unsigned int value) {
  unsigned int *p = FRT_ALLOC(unsigned int);
  *p = value;
  return p;
}

unsigned long *frt_lmalloc(unsigned long value) {
  unsigned long *p = FRT_ALLOC(unsigned long);
  *p = value;
  return p;
}

frt_u32 *frt_u32malloc(frt_u32 value) {
  frt_u32 *p = FRT_ALLOC(frt_u32);
  *p = value;
  return p;
}

frt_u64 *frt_u64malloc(frt_u64 value) {
  frt_u64 *p = FRT_ALLOC(frt_u64);
  *p = value;
  return p;
}

/* concatenate two strings freeing the second */
char *frt_estrcat(char *str1, char *str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    FRT_REALLOC_N(str1, char, len1 + len2 + 3);     /* leave room for <CR> */
    memcpy(str1 + len1, str2, len2 + 1);        /* make sure '\0' copied too */
    free(str2);
    return str1;
}

/* epstrdup: duplicate a string with a format, report if error */
char *frt_epstrdup(const char *fmt, int len, ...) {
    char *string;
    va_list args;
    len += (int) strlen(fmt);

    string = FRT_ALLOC_N(char, len + 1);
    va_start(args, len);
    vsprintf(string, fmt, args);
    va_end(args);

    return string;
}

/* frt_estrdup: duplicate a string, report if error */
char *frt_estrdup(const char *s) {
    char *t = FRT_ALLOC_N(char, strlen(s) + 1);
    strcpy(t, s);
    return t;
}

/* Pretty print a float to the buffer. The buffer should have at least 32
 * bytes available.
 */
char *frt_dbl_to_s(char *buf, double num) {
    char *p, *e;

    if (isinf(num)) {
        if (num < 0) {
            return strcpy(buf, "-Infinity");
        } else {
            return strcpy(buf, "Infinity");
        }
    } else if (isnan(num)) {
        return strcpy(buf, "NaN");
    }

    sprintf(buf, FRT_DBL2S, num);
    if (!(e = strchr(buf, 'e'))) {
        e = buf + strlen(buf);
    }
    if (!isdigit(e[-1])) {
        /* reformat if ended with decimal point (ex 111111111111111.) */
        sprintf(buf, "%#.6e", num);
        if (!(e = strchr(buf, 'e'))) { e = buf + strlen(buf); }
    }
    p = e;
    while (p[-1] == '0' && isdigit(p[-2])) {
        p--;
    }

    memmove(p, e, strlen(e) + 1);
    return buf;
}

/**
 * frt_strapp: appends a string up to, but not including the \0 character to the
 * end of a string returning a pointer to the next unassigned character in the
 * string.
 */
char *frt_strapp(char *dst, const char *src) {
    while (*src != '\0') {
        *dst = *src;
        ++dst;
        ++src;
    }
    return dst;
}

/* strfmt: like sprintf except that it allocates memory for the string */
char *frt_vstrfmt(const char *fmt, va_list args) {
    char *string;
    char *p = (char *) fmt, *q;
    int len = (int) strlen(fmt) + 1;
    int slen, curlen;
    const char *s;
    long l;
    double d;

    q = string = FRT_ALLOC_N(char, len);

    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
            case 's':
                p++;
                s = va_arg(args, char *);
                /* to be consistent with printf print (null) for NULL */
                if (!s) {
                    s = "(null)";
                }
                slen = (int) strlen(s);
                len += slen;
                curlen = q - string;
                FRT_REALLOC_N(string, char, len);
                q = string + curlen;
                memcpy(q, s, slen);
                q += slen;
                continue;
            case 'f':
                p++;
                len += 32;
                *q = 0;
                FRT_REALLOC_N(string, char, len);
                q = string + strlen(string);
                d = va_arg(args, double);
                frt_dbl_to_s(q, d);
                q += strlen(q);
                continue;
            case 'd':
                p++;
                len += 20;
                *q = 0;
                FRT_REALLOC_N(string, char, len);
                q = string + strlen(string);
                l = va_arg(args, long);
                q += sprintf(q, "%ld", l);
                continue;
            default:
                break;
            }
        }
        *q = *p;
        p++;
        q++;
    }
    *q = 0;

    return string;
}

char *frt_strfmt(const char *fmt, ...) {
    va_list args;
    char *str;
    va_start(args, fmt);
    str = frt_vstrfmt(fmt, args);
    va_end(args);
    return str;
}

void frt_dummy_free(void *p) {
    (void)p; /* suppress unused argument warning */
}

typedef struct FreeMe {
    void *p;
    frt_free_ft free_func;
} FreeMe;

static FreeMe *free_mes = NULL;
static int free_mes_size = 0;
static int free_mes_capa = 0;

void frt_register_for_cleanup(void *p, frt_free_ft free_func) {
    FreeMe *free_me;
    if (free_mes_capa == 0) {
        free_mes_capa = 16;
        free_mes = FRT_ALLOC_N(FreeMe, free_mes_capa);
    }
    else if (free_mes_capa <= free_mes_size) {
        free_mes_capa *= 2;
        FRT_REALLOC_N(free_mes, FreeMe, free_mes_capa);
    }
    free_me = free_mes + free_mes_size++;
    free_me->p = p;
    free_me->free_func = free_func;
}

void frt_init(int argc, const char *const argv[]) {
    atexit(&frt_hash_finalize);

    utf8_encoding = rb_utf8_encoding();
    utf8_mbmaxlen = rb_enc_mbmaxlen(utf8_encoding);
    char *p = "'";
    cp_apostrophe = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ".";
    cp_dot = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ",";
    cp_comma = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "\\";
    cp_backslash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "/";
    cp_slash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "_";
    cp_underscore = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "-";
    cp_dash = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "\u2010";
    cp_hyphen = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "@";
    cp_at = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = "&";
    cp_ampersand = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);
    p = ":";
    cp_colon = rb_enc_mbc_to_codepoint(p, p + 1, utf8_encoding);

    FRT_SORT_FIELD_SCORE = frt_sort_field_alloc();
    FRT_SORT_FIELD_SCORE->field_index_class = NULL;               /* field_index_class */
    FRT_SORT_FIELD_SCORE->field = (ID)NULL;                       /* field */
    FRT_SORT_FIELD_SCORE->type = FRT_SORT_TYPE_SCORE;             /* type */
    FRT_SORT_FIELD_SCORE->reverse = false;                        /* reverse */
    FRT_SORT_FIELD_SCORE->compare = frt_sort_field_score_compare; /* compare */
    FRT_SORT_FIELD_SCORE->get_val = frt_sort_field_score_get_val; /* get_val */

    FRT_SORT_FIELD_SCORE_REV = frt_sort_field_alloc();
    FRT_SORT_FIELD_SCORE_REV->field_index_class = NULL;               /* field_index_class */
    FRT_SORT_FIELD_SCORE_REV->field = (ID)NULL;                       /* field */
    FRT_SORT_FIELD_SCORE_REV->type = FRT_SORT_TYPE_SCORE;             /* type */
    FRT_SORT_FIELD_SCORE_REV->reverse = true;                         /* reverse */
    FRT_SORT_FIELD_SCORE_REV->compare = frt_sort_field_score_compare; /* compare */
    FRT_SORT_FIELD_SCORE_REV->get_val = frt_sort_field_score_get_val; /* get_val */

    FRT_SORT_FIELD_DOC = frt_sort_field_alloc();
    FRT_SORT_FIELD_DOC->field_index_class = NULL;             /* field_index_class */
    FRT_SORT_FIELD_DOC->field = (ID)NULL;                     /* field */
    FRT_SORT_FIELD_DOC->type = FRT_SORT_TYPE_DOC;             /* type */
    FRT_SORT_FIELD_DOC->reverse = false;                      /* reverse */
    FRT_SORT_FIELD_DOC->compare = frt_sort_field_doc_compare; /* compare */
    FRT_SORT_FIELD_DOC->get_val = frt_sort_field_doc_get_val; /* get_val */

    FRT_SORT_FIELD_DOC_REV = frt_sort_field_alloc();
    FRT_SORT_FIELD_DOC_REV->field_index_class = NULL;             /* field_index_class */
    FRT_SORT_FIELD_DOC_REV->field = (ID)NULL;                     /* field */
    FRT_SORT_FIELD_DOC_REV->type = FRT_SORT_TYPE_DOC;             /* type */
    FRT_SORT_FIELD_DOC_REV->reverse = true;                       /* reverse */
    FRT_SORT_FIELD_DOC_REV->compare = frt_sort_field_doc_compare; /* compare */
    FRT_SORT_FIELD_DOC_REV->get_val = frt_sort_field_doc_get_val; /* get_val */
}
