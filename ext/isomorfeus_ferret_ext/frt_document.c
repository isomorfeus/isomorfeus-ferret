#include "ruby.h"
#include "frt_document.h"
#include <string.h>
#include "frt_internal.h"

/****************************************************************************
 *
 * FrtDocField
 *
 ****************************************************************************/

FrtDocField *df_new(FrtSymbol name)
{
    FrtDocField *df = FRT_ALLOC(FrtDocField);
    df->name = name;
    df->size = 0;
    df->capa = FRT_DF_INIT_CAPA;
    df->data = FRT_ALLOC_N(char *, df->capa);
    df->lengths = FRT_ALLOC_N(int, df->capa);
    df->destroy_data = false;
    df->boost = 1.0f;
    return df;
}

FrtDocField *df_add_data_len(FrtDocField *df, char *data, int len)
{
    if (df->size >= df->capa) {
        df->capa <<= 2;
        FRT_REALLOC_N(df->data, char *, df->capa);
        FRT_REALLOC_N(df->lengths, int, df->capa);
    }
    df->data[df->size] = data;
    df->lengths[df->size] = len;
    df->size++;
    return df;
}

FrtDocField *df_add_data(FrtDocField *df, char *data)
{
    return df_add_data_len(df, data, strlen(data));
}

void df_destroy(FrtDocField *df)
{
    if (df->destroy_data) {
        int i;
        for (i = 0; i < df->size; i++) {
            free(df->data[i]);
        }
    }
    free(df->data);
    free(df->lengths);
    free(df);
}

/*
 * Format for one item is: name: "data"
 *        for more items : name: ["data", "data", "data"]
 */
char *df_to_s(FrtDocField *df)
{
    int i, len = 0, namelen = strlen(df->name);
    char *str, *s;
    for (i = 0; i < df->size; i++) {
        len += df->lengths[i] + 4;
    }
    s = str = FRT_ALLOC_N(char, namelen + len + 5);
    memcpy(s, df->name, namelen);
    s += namelen;
    s = strapp(s, ": ");

    if (df->size > 1) {
        s = strapp(s, "[");
    }
    for (i = 0; i < df->size; i++) {
        if (i != 0) {
            s = strapp(s, ", ");
        }
        s = strapp(s, "\"");
        memcpy(s, df->data[i], df->lengths[i]);
        s += df->lengths[i];
        s = strapp(s, "\"");
    }

    if (df->size > 1) {
        s = strapp(s, "]");
    }
    *s = 0;
    return str;
}

/****************************************************************************
 *
 * FrtDocument
 *
 ****************************************************************************/

FrtDocument *doc_new()
{
    FrtDocument *doc = FRT_ALLOC(FrtDocument);
    doc->field_dict = h_new_str(NULL, (free_ft)&df_destroy);
    doc->size = 0;
    doc->capa = FRT_DOC_INIT_CAPA;
    doc->fields = FRT_ALLOC_N(FrtDocField *, doc->capa);
    doc->boost = 1.0f;
    return doc;
}

FrtDocField *doc_add_field(FrtDocument *doc, FrtDocField *df)
{
    if (!h_set_safe(doc->field_dict, df->name, df)) {
        rb_raise(rb_eException, "tried to add %s field which alread existed\n",
              df->name);
    }
    if (doc->size >= doc->capa) {
        doc->capa <<= 1;
        FRT_REALLOC_N(doc->fields, FrtDocField *, doc->capa);
    }
    doc->fields[doc->size] = df;
    doc->size++;
    return df;
}

FrtDocField *doc_get_field(FrtDocument *doc, FrtSymbol name)
{
    return (FrtDocField *)h_get(doc->field_dict, name);
}

char *doc_to_s(FrtDocument *doc)
{
    int i;
    int len = 0;
    char **fields = FRT_ALLOC_N(char *, doc->size);
    char *buf, *s;

    for (i = 0; i < doc->size; i++) {
        fields[i] = df_to_s(doc->fields[i]);
        len += strlen(fields[i]) + 5;
    }
    s = buf = FRT_ALLOC_N(char, len + 12);
    s += sprintf(buf, "Document [\n");
    for (i = 0; i < doc->size; i++) {
        s += sprintf(s, "  =>%s\n", fields[i]);
        free(fields[i]);
    }
    free(fields);
    return buf;
}

void doc_destroy(FrtDocument *doc)
{
    h_destroy(doc->field_dict);
    free(doc->fields);
    free(doc);
}

