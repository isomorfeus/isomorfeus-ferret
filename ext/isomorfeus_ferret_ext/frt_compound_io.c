#include "ruby.h"
#include "frt_index.h"
#include "frt_array.h"
#include "frt_internal.h"

extern VALUE cStateError;
extern void store_destroy(FrtStore *store);
extern FrtInStream *is_new();
extern FrtStore *store_new();

/****************************************************************************
 *
 * CompoundStore
 *
 ****************************************************************************/

typedef struct FileEntry {
    off_t offset;
    off_t length;
} FileEntry;

static void cmpd_touch(FrtStore *store, const char *file_name)
{
    store->dir.cmpd->store->touch(store->dir.cmpd->store, file_name);
}

static int cmpd_exists(FrtStore *store, const char *file_name)
{
    if (h_get(store->dir.cmpd->entries, file_name) != NULL) {
        return true;
    }
    else {
        return false;
    }
}

static int cmpd_remove(FrtStore *store, const char *file_name)
{
    (void)store;
    (void)file_name;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return 0;
}

static void cmpd_rename(FrtStore *store, const char *from, const char *to)
{
    (void)store;
    (void)from;
    (void)to;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
}

static int cmpd_count(FrtStore *store)
{
    return store->dir.cmpd->entries->size;
}

static void cmpd_each(FrtStore *store,
                     void (*func)(const char *fname, void *arg), void *arg)
{
    FrtHash *ht = store->dir.cmpd->entries;
    int i;
    for (i = 0; i <= ht->mask; i++) {
        char *fn = (char *)ht->table[i].key;
        if (fn) {
            func(fn, arg);
        }
    }
}


static void cmpd_clear(FrtStore *store)
{
    (void)store;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
}

static void cmpd_close_i(FrtStore *store)
{
    FrtCompoundStore *cmpd = store->dir.cmpd;
    if (cmpd->stream == NULL) {
        rb_raise(rb_eIOError, "Tried to close already closed compound store");
    }

    h_destroy(cmpd->entries);

    is_close(cmpd->stream);
    cmpd->stream = NULL;
    free(store->dir.cmpd);
    store_destroy(store);
}

static off_t cmpd_length(FrtStore *store, const char *file_name)
{
    FileEntry *fe = (FileEntry *)h_get(store->dir.cmpd->entries, file_name);
    if (fe != NULL) {
        return fe->length;
    }
    else {
        return 0;
    }
}

static void cmpdi_seek_i(FrtInStream *is, off_t pos)
{
    (void)is;
    (void)pos;
}

static void cmpdi_close_i(FrtInStream *is)
{
    free(is->d.cis);
}

static off_t cmpdi_length_i(FrtInStream *is)
{
    return (is->d.cis->length);
}

/*
 * raises: FRT_EOF_ERROR
 */
static void cmpdi_read_i(FrtInStream *is, uchar *b, int len)
{
    FrtCompoundInStream *cis = is->d.cis;
    off_t start = is_pos(is);

    if ((start + len) > cis->length) {
        rb_raise(rb_eEOFError, "Tried to read past end of file. File length is "
              "<%"FRT_OFF_T_PFX"d> and tried to read to <%"FRT_OFF_T_PFX"d>",
              cis->length, start + len);
    }

    is_seek(cis->sub, cis->offset + start);
    is_read_bytes(cis->sub, b, len);
}

static const struct FrtInStreamMethods CMPD_IN_STREAM_METHODS = {
    cmpdi_read_i,
    cmpdi_seek_i,
    cmpdi_length_i,
    cmpdi_close_i
};

static FrtInStream *cmpd_create_input(FrtInStream *sub_is, off_t offset, off_t length)
{
    FrtInStream *is = is_new();
    FrtCompoundInStream *cis = FRT_ALLOC(FrtCompoundInStream);

    cis->sub = sub_is;
    cis->offset = offset;
    cis->length = length;
    is->d.cis = cis;
    is->m = &CMPD_IN_STREAM_METHODS;

    return is;
}

static FrtInStream *cmpd_open_input(FrtStore *store, const char *file_name)
{
    FileEntry *entry;
    FrtCompoundStore *cmpd = store->dir.cmpd;
    FrtInStream *is;

    mutex_lock(&store->mutex);
    if (cmpd->stream == NULL) {
        mutex_unlock(&store->mutex);
        rb_raise(rb_eIOError, "Can't open compound file input stream. Parent "
              "stream is closed.");
    }

    entry = (FileEntry *)h_get(cmpd->entries, file_name);
    if (entry == NULL) {
        mutex_unlock(&store->mutex);
        rb_raise(rb_eIOError, "File %s does not exist: ", file_name);
    }

    is = cmpd_create_input(cmpd->stream, entry->offset, entry->length);
    mutex_unlock(&store->mutex);

    return is;
}

static FrtOutStream *cmpd_new_output(FrtStore *store, const char *file_name)
{
    (void)store;
    (void)file_name;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static FrtLock *cmpd_open_lock_i(FrtStore *store, const char *lock_name)
{
    (void)store;
    (void)lock_name;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
    return NULL;
}

static void cmpd_close_lock_i(FrtLock *lock)
{
    (void)lock;
    rb_raise(rb_eNotImpError, "%s", FRT_UNSUPPORTED_ERROR_MSG);
}

FrtStore *open_cmpd_store(FrtStore *store, const char *name)
{
    int count, i;
    off_t offset;
    char *fname;
    FileEntry *volatile entry = NULL;
    FrtStore *new_store = NULL;
    FrtCompoundStore *volatile cmpd = NULL;
    FrtInStream *volatile is = NULL;

    FRT_TRY
        cmpd = FRT_ALLOC_AND_ZERO(FrtCompoundStore);

        cmpd->store       = store;
        cmpd->name        = name;
        cmpd->entries     = h_new_str(&free, &free);
        is = cmpd->stream = store->open_input(store, cmpd->name);

        /* read the directory and init files */
        count = is_read_vint(is);
        entry = NULL;
        for (i = 0; i < count; i++) {
            offset = (off_t)is_read_i64(is);
            fname = is_read_string(is);

            if (entry != NULL) {
                /* set length of the previous entry */
                entry->length = offset - entry->offset;
            }

            entry = FRT_ALLOC(FileEntry);
            entry->offset = offset;
            h_set(cmpd->entries, fname, entry);
        }
    FRT_XCATCHALL
        if (is) is_close(is);
        if (cmpd->entries) h_destroy(cmpd->entries);
        free(cmpd);
    FRT_XENDTRY

    /* set the length of the final entry */
    if (entry != NULL) {
        entry->length = is_length(is) - entry->offset;
    }

    new_store               = store_new();
    new_store->dir.cmpd     = cmpd;
    new_store->touch        = &cmpd_touch;
    new_store->exists       = &cmpd_exists;
    new_store->remove       = &cmpd_remove;
    new_store->rename       = &cmpd_rename;
    new_store->count        = &cmpd_count;
    new_store->clear        = &cmpd_clear;
    new_store->length       = &cmpd_length;
    new_store->each         = &cmpd_each;
    new_store->close_i      = &cmpd_close_i;
    new_store->new_output   = &cmpd_new_output;
    new_store->open_input   = &cmpd_open_input;
    new_store->open_lock_i  = &cmpd_open_lock_i;
    new_store->close_lock_i = &cmpd_close_lock_i;

    return new_store;
}

/****************************************************************************
 *
 * CompoundWriter
 *
 ****************************************************************************/

FrtCompoundWriter *open_cw(FrtStore *store, char *name)
{
    FrtCompoundWriter *cw = FRT_ALLOC(FrtCompoundWriter);
    cw->store = store;
    cw->name = name;
    cw->ids = hs_new_str(&free);
    cw->file_entries = frt_ary_new_type_capa(FrtCWFileEntry, FRT_CW_INIT_CAPA);
    return cw;
}

void frt_cw_add_file(FrtCompoundWriter *cw, char *id)
{
    id = frt_estrdup(id);
    if (hs_add(cw->ids, id) != FRT_HASH_KEY_DOES_NOT_EXIST) {
        rb_raise(rb_eIOError, "Tried to add file \"%s\" which has already been "
              "added to the compound store", id);
    }

    frt_ary_grow(cw->file_entries);
    frt_ary_last(cw->file_entries).name = id;
}

static void cw_copy_file(FrtCompoundWriter *cw, FrtCWFileEntry *src, FrtOutStream *os)
{
    off_t start_ptr = os_pos(os);
    off_t end_ptr;
    off_t remainder, length, len;
    uchar buffer[FRT_BUFFER_SIZE];

    FrtInStream *is = cw->store->open_input(cw->store, src->name);

    remainder = length = is_length(is);

    while (remainder > 0) {
        len = FRT_MIN(remainder, FRT_BUFFER_SIZE);
        is_read_bytes(is, buffer, len);
        os_write_bytes(os, buffer, len);
        remainder -= len;
    }

    /* Verify that remainder is 0 */
    if (remainder != 0) {
        rb_raise(rb_eIOError, "There seems to be an error in the compound file "
              "should have read to the end but there are <%"FRT_OFF_T_PFX"d> "
              "bytes left", remainder);
    }

    /* Verify that the output length diff is equal to original file */
    end_ptr = os_pos(os);
    len = end_ptr - start_ptr;
    if (len != length) {
        rb_raise(rb_eIOError, "Difference in compound file output file offsets "
              "<%"FRT_OFF_T_PFX"d> does not match the original file lenght "
              "<%"FRT_OFF_T_PFX"d>", len, length);
    }

    is_close(is);
}

void frt_cw_close(FrtCompoundWriter *cw)
{
    FrtOutStream *os = NULL;
    int i;

    if (cw->ids->size <= 0) {
        rb_raise(cStateError, "Tried to merge compound file with no entries");
    }

    os = cw->store->new_output(cw->store, cw->name);

    os_write_vint(os, frt_ary_size(cw->file_entries));

    /* Write the directory with all offsets at 0.
     * Remember the positions of directory entries so that we can adjust the
     * offsets later */
    for (i = 0; i < frt_ary_size(cw->file_entries); i++) {
        cw->file_entries[i].dir_offset = os_pos(os);
        os_write_u64(os, 0);  /* for now */
        os_write_string(os, cw->file_entries[i].name);
    }

    /* Open the files and copy their data into the stream.  Remember the
     * locations of each file's data section. */
    for (i = 0; i < frt_ary_size(cw->file_entries); i++) {
        cw->file_entries[i].data_offset = os_pos(os);
        cw_copy_file(cw, &cw->file_entries[i], os);
    }

    /* Write the data offsets into the directory of the compound stream */
    for (i = 0; i < frt_ary_size(cw->file_entries); i++) {
        os_seek(os, cw->file_entries[i].dir_offset);
        os_write_u64(os, cw->file_entries[i].data_offset);
    }

    if (os) {
        os_close(os);
    }

    hs_destroy(cw->ids);
    frt_ary_free(cw->file_entries);
    free(cw);
}
