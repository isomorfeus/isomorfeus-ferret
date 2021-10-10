#include "ruby.h"
#include "frt_store.h"
#include <string.h>
#include "frt_internal.h"

extern VALUE cFileNotFoundError;

static FrtRAMFile *rf_new(const char *name)
{
    FrtRAMFile *rf = FRT_ALLOC(FrtRAMFile);
    rf->buffers = FRT_ALLOC(uchar *);
    rf->buffers[0] = FRT_ALLOC_N(uchar, FRT_BUFFER_SIZE);
    rf->name = estrdup(name);
    rf->len = 0;
    rf->bufcnt = 1;
    rf->ref_cnt = 1;
    return rf;
}

static void rf_extend_if_necessary(FrtRAMFile *rf, int buf_num)
{
    while (rf->bufcnt <= buf_num) {
        FRT_REALLOC_N(rf->buffers, uchar *, (rf->bufcnt + 1));
        rf->buffers[rf->bufcnt++] = FRT_ALLOC_N(uchar, FRT_BUFFER_SIZE);
    }
}

static void rf_close(void *p)
{
    int i;
    FrtRAMFile *rf = (FrtRAMFile *)p;
    if (rf->ref_cnt > 0) {
        return;
    }
    free(rf->name);
    for (i = 0; i < rf->bufcnt; i++) {
        free(rf->buffers[i]);
    }
    free(rf->buffers);
    free(rf);
}

static void ram_touch(FrtStore *store, const char *filename)
{
    if (h_get(store->dir.ht, filename) == NULL) {
        h_set(store->dir.ht, filename, rf_new(filename));
    }
}

static int ram_exists(FrtStore *store, const char *filename)
{
    if (h_get(store->dir.ht, filename) != NULL) {
        return true;
    }
    else {
        return false;
    }
}

static int ram_remove(FrtStore *store, const char *filename)
{
    FrtRAMFile *rf = (FrtRAMFile *)h_rem(store->dir.ht, filename, false);
    if (rf != NULL) {
        FRT_DEREF(rf);
        rf_close(rf);
        return true;
    }
    else {
        return false;
    }
}

static void ram_rename(FrtStore *store, const char *from, const char *to)
{
    FrtRAMFile *rf = (FrtRAMFile *)h_rem(store->dir.ht, from, false);
    FrtRAMFile *tmp;

    if (rf == NULL) {
        rb_raise(rb_eIOError, "couldn't rename \"%s\" to \"%s\". \"%s\""
              " doesn't exist", from, to, from);
    }

    free(rf->name);

    rf->name = estrdup(to);

    /* clean up the file we are overwriting */
    tmp = (FrtRAMFile *)h_get(store->dir.ht, to);
    if (tmp != NULL) {
        FRT_DEREF(tmp);
    }

    h_set(store->dir.ht, rf->name, rf);
}

static int ram_count(FrtStore *store)
{
    return store->dir.ht->size;
}

static void ram_each(FrtStore *store,
                     void (*func)(const char *fname, void *arg), void *arg)
{
    FrtHash *ht = store->dir.ht;
    int i;
    for (i = 0; i <= ht->mask; i++) {
        FrtRAMFile *rf = (FrtRAMFile *)ht->table[i].value;
        if (rf) {
            if (strncmp(rf->name, FRT_LOCK_PREFIX, strlen(FRT_LOCK_PREFIX)) == 0) {
                continue;
            }
            func(rf->name, arg);
        }
    }
}

static void ram_close_i(FrtStore *store)
{
    FrtHash *ht = store->dir.ht;
    int i;
    for (i = 0; i <= ht->mask; i++) {
        FrtRAMFile *rf = (FrtRAMFile *)ht->table[i].value;
        if (rf) {
            FRT_DEREF(rf);
        }
    }
    h_destroy(store->dir.ht);
    store_destroy(store);
}

/*
 * Be sure to keep the locks
 */
static void ram_clear(FrtStore *store)
{
    int i;
    FrtHash *ht = store->dir.ht;
    for (i = 0; i <= ht->mask; i++) {
        FrtRAMFile *rf = (FrtRAMFile *)ht->table[i].value;
        if (rf && !file_is_lock(rf->name)) {
            FRT_DEREF(rf);
            h_del(ht, rf->name);
        }
    }
}

static void ram_clear_locks(FrtStore *store)
{
    int i;
    FrtHash *ht = store->dir.ht;
    for (i = 0; i <= ht->mask; i++) {
        FrtRAMFile *rf = (FrtRAMFile *)ht->table[i].value;
        if (rf && file_is_lock(rf->name)) {
            FRT_DEREF(rf);
            h_del(ht, rf->name);
        }
    }
}

static void ram_clear_all(FrtStore *store)
{
    int i;
    FrtHash *ht = store->dir.ht;
    for (i = 0; i <= ht->mask; i++) {
        FrtRAMFile *rf = (FrtRAMFile *)ht->table[i].value;
        if (rf) {
            FRT_DEREF(rf);
            h_del(ht, rf->name);
        }
    }
}

static off_t ram_length(FrtStore *store, const char *filename)
{
    FrtRAMFile *rf = (FrtRAMFile *)h_get(store->dir.ht, filename);
    if (rf != NULL) {
        return rf->len;
    }
    else {
        return 0;
    }
}

off_t ramo_length(FrtOutStream *os)
{
    return os->file.rf->len;
}

static void ramo_flush_i(FrtOutStream *os, const uchar *src, int len)
{
    uchar *buffer;
    FrtRAMFile *rf = os->file.rf;
    int buffer_number, buffer_offset, bytes_in_buffer, bytes_to_copy;
    int src_offset;
    off_t pointer = os->pointer;

    buffer_number = (int)(pointer / FRT_BUFFER_SIZE);
    buffer_offset = pointer % FRT_BUFFER_SIZE;
    bytes_in_buffer = FRT_BUFFER_SIZE - buffer_offset;
    bytes_to_copy = bytes_in_buffer < len ? bytes_in_buffer : len;

    rf_extend_if_necessary(rf, buffer_number);

    buffer = rf->buffers[buffer_number];
    memcpy(buffer + buffer_offset, src, bytes_to_copy);

    if (bytes_to_copy < len) {
        src_offset = bytes_to_copy;
        bytes_to_copy = len - bytes_to_copy;
        buffer_number += 1;
        rf_extend_if_necessary(rf, buffer_number);
        buffer = rf->buffers[buffer_number];

        memcpy(buffer, src + src_offset, bytes_to_copy);
    }
    os->pointer += len;

    if (os->pointer > rf->len) {
        rf->len = os->pointer;
    }
}

static void ramo_seek_i(FrtOutStream *os, off_t pos)
{
    os->pointer = pos;
}

void ramo_reset(FrtOutStream *os)
{
    os_seek(os, 0);
    os->file.rf->len = 0;
}

static void ramo_close_i(FrtOutStream *os)
{
    FrtRAMFile *rf = os->file.rf;
    FRT_DEREF(rf);
    rf_close(rf);
}

void ramo_write_to(FrtOutStream *os, FrtOutStream *other_o)
{
    int i, len;
    FrtRAMFile *rf = os->file.rf;
    int last_buffer_number;
    int last_buffer_offset;

    os_flush(os);
    last_buffer_number = (int) (rf->len / FRT_BUFFER_SIZE);
    last_buffer_offset = rf->len % FRT_BUFFER_SIZE;
    for (i = 0; i <= last_buffer_number; i++) {
        len = (i == last_buffer_number ? last_buffer_offset : FRT_BUFFER_SIZE);
        os_write_bytes(other_o, rf->buffers[i], len);
    }
}

static const struct FrtOutStreamMethods RAM_OUT_STREAM_METHODS = {
    ramo_flush_i,
    ramo_seek_i,
    ramo_close_i
};

FrtOutStream *ram_new_buffer()
{
    FrtRAMFile *rf = rf_new("");
    FrtOutStream *os = os_new();

    FRT_DEREF(rf);
    os->file.rf = rf;
    os->pointer = 0;
    os->m = &RAM_OUT_STREAM_METHODS;
    return os;
}

void ram_destroy_buffer(FrtOutStream *os)
{
    rf_close(os->file.rf);
    free(os);
}

static FrtOutStream *ram_new_output(FrtStore *store, const char *filename)
{
    FrtRAMFile *rf = (FrtRAMFile *)h_get(store->dir.ht, filename);
    FrtOutStream *os = os_new();

    if (rf == NULL) {
        rf = rf_new(filename);
        h_set(store->dir.ht, rf->name, rf);
    }
    FRT_REF(rf);
    os->pointer = 0;
    os->file.rf = rf;
    os->m = &RAM_OUT_STREAM_METHODS;
    return os;
}

static void rami_read_i(FrtInStream *is, uchar *b, int len)
{
    FrtRAMFile *rf = is->file.rf;

    int offset = 0;
    int buffer_number, buffer_offset, bytes_in_buffer, bytes_to_copy;
    int remainder = len;
    off_t start = is->d.pointer;
    uchar *buffer;

    while (remainder > 0) {
        buffer_number = (int) (start / FRT_BUFFER_SIZE);
        buffer_offset = start % FRT_BUFFER_SIZE;
        bytes_in_buffer = FRT_BUFFER_SIZE - buffer_offset;

        if (bytes_in_buffer >= remainder) {
            bytes_to_copy = remainder;
        }
        else {
            bytes_to_copy = bytes_in_buffer;
        }
        buffer = rf->buffers[buffer_number];
        memcpy(b + offset, buffer + buffer_offset, bytes_to_copy);
        offset += bytes_to_copy;
        start += bytes_to_copy;
        remainder -= bytes_to_copy;
    }

    is->d.pointer += len;
}

static off_t rami_length_i(FrtInStream *is)
{
    return is->file.rf->len;
}

static void rami_seek_i(FrtInStream *is, off_t pos)
{
    is->d.pointer = pos;
}

static void rami_close_i(FrtInStream *is)
{
    FrtRAMFile *rf = is->file.rf;
    FRT_DEREF(rf);
    rf_close(rf);
}

static const struct FrtInStreamMethods RAM_IN_STREAM_METHODS = {
    rami_read_i,
    rami_seek_i,
    rami_length_i,
    rami_close_i
};

static FrtInStream *ram_open_input(FrtStore *store, const char *filename)
{
    FrtRAMFile *rf = (FrtRAMFile *)h_get(store->dir.ht, filename);
    FrtInStream *is = NULL;

    if (rf == NULL) {
        /*
        FrtHash *ht = store->dir.ht;
        int i;
        printf("\nlooking for %s, %ld\n", filename, str_hash(filename));
        for (i = 0; i <= ht->mask; i++) {
            if (ht->table[i].value)
                printf("%s, %ld  --  %ld\n", (char *)ht->table[i].key,
                       str_hash(ht->table[i].key), ht->table[i].hash);
        }
        */
        rb_raise(cFileNotFoundError,
              "tried to open \"%s\" but it doesn't exist", filename);
    }
    FRT_REF(rf);
    is = is_new();
    is->file.rf = rf;
    is->d.pointer = 0;
    is->m = &RAM_IN_STREAM_METHODS;

    return is;
}

#define LOCK_OBTAIN_TIMEOUT 5

static int ram_lock_obtain(FrtLock *lock)
{
    int ret = true;
    if (ram_exists(lock->store, lock->name)) {
        ret = false;
    }
    ram_touch(lock->store, lock->name);
    return ret;
}

static int ram_lock_is_locked(FrtLock *lock)
{
    return ram_exists(lock->store, lock->name);
}

static void ram_lock_release(FrtLock *lock)
{
    ram_remove(lock->store, lock->name);
}

static FrtLock *ram_open_lock_i(FrtStore *store, const char *lockname)
{
    FrtLock *lock = FRT_ALLOC(FrtLock);
    char lname[100];
    snprintf(lname, 100, "%s%s.lck", FRT_LOCK_PREFIX, lockname);
    lock->name = estrdup(lname);
    lock->store = store;
    lock->obtain = &ram_lock_obtain;
    lock->release = &ram_lock_release;
    lock->is_locked = &ram_lock_is_locked;
    return lock;
}

static void ram_close_lock_i(FrtLock *lock)
{
    free(lock->name);
    free(lock);
}


FrtStore *open_ram_store()
{
    FrtStore *new_store = store_new();

    new_store->dir.ht       = h_new_str(NULL, rf_close);
    new_store->touch        = &ram_touch;
    new_store->exists       = &ram_exists;
    new_store->remove       = &ram_remove;
    new_store->rename       = &ram_rename;
    new_store->count        = &ram_count;
    new_store->clear        = &ram_clear;
    new_store->clear_all    = &ram_clear_all;
    new_store->clear_locks  = &ram_clear_locks;
    new_store->length       = &ram_length;
    new_store->each         = &ram_each;
    new_store->new_output   = &ram_new_output;
    new_store->open_input   = &ram_open_input;
    new_store->open_lock_i  = &ram_open_lock_i;
    new_store->close_lock_i = &ram_close_lock_i;
    new_store->close_i      = &ram_close_i;
    return new_store;
}

struct CopyFileArg
{
    FrtStore *to_store, *from_store;
};

static void copy_files(const char *fname, void *arg)
{
    struct CopyFileArg *cfa = (struct CopyFileArg *)arg;
    FrtOutStream *os = cfa->to_store->new_output(cfa->to_store, fname);
    FrtInStream *is = cfa->from_store->open_input(cfa->from_store, fname);
    int len = (int)is_length(is);
    uchar *buffer = FRT_ALLOC_N(uchar, len + 1);

    is_read_bytes(is, buffer, len);
    os_write_bytes(os, buffer, len);

    is_close(is);
    os_close(os);
    free(buffer);
}

FrtStore *open_ram_store_and_copy(FrtStore *from_store, bool close_dir)
{
    FrtStore *store = open_ram_store();
    struct CopyFileArg cfa;
    cfa.to_store = store;
    cfa.from_store = from_store;

    from_store->each(from_store, &copy_files, &cfa);

    if (close_dir) {
        store_deref(from_store);
    }

    return store;
}
