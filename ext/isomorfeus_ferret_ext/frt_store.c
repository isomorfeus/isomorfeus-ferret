#include "ruby.h"
#include "frt_store.h"
#include <string.h>
#include "frt_internal.h"

#define VINT_MAX_LEN 10
#define VINT_END FRT_BUFFER_SIZE - VINT_MAX_LEN

extern VALUE cLockError;

void store_deref(FrtStore *store)
{
    mutex_lock(&store->mutex_i);
    if (--store->ref_cnt <= 0) {
        store->close_i(store);
    }
    else {
        mutex_unlock(&store->mutex_i);
    }
}

FrtLock *open_lock(FrtStore *store, const char *lockname)
{
    FrtLock *lock = store->open_lock_i(store, lockname);
    hs_add(store->locks, lock);
    return lock;
}

void frt_close_lock(FrtLock *lock)
{
    hs_del(lock->store->locks, lock);
}

static void frt_close_lock_i(FrtLock *lock)
{
    lock->store->close_lock_i(lock);
}

/**
 * Create a store struct initializing the mutex.
 */
FrtStore *store_new()
{
    FrtStore *store = FRT_ALLOC(FrtStore);
    store->ref_cnt = 1;
    mutex_init(&store->mutex_i, NULL);
    mutex_init(&store->mutex, NULL);
    store->locks = hs_new_ptr((free_ft)&frt_close_lock_i);
    return store;
}

/**
 * Destroy the store freeing allocated resources
 *
 * @param store the store struct to free
 */
void store_destroy(FrtStore *store)
{
    mutex_destroy(&store->mutex_i);
    mutex_destroy(&store->mutex);
    hs_destroy(store->locks);
    free(store);
}

/**
 * Create a newly allocated and initialized OutStream object
 *
 * @return a newly allocated and initialized OutStream object
 */
FrtOutStream *os_new()
{
    FrtOutStream *os = FRT_ALLOC(FrtOutStream);
    os->buf.start = 0;
    os->buf.pos = 0;
    os->buf.len = 0;
    return os;
}

/**
 * Flush the countents of the FrtOutStream's buffers
 *
 * @param the OutStream to flush
 */
void os_flush(FrtOutStream *os)
{
    os->m->flush_i(os, os->buf.buf, os->buf.pos);
    os->buf.start += os->buf.pos;
    os->buf.pos = 0;
}

void os_close(FrtOutStream *os)
{
    os_flush(os);
    os->m->close_i(os);
    free(os);
}

off_t os_pos(FrtOutStream *os)
{
    return os->buf.start + os->buf.pos;
}

void os_seek(FrtOutStream *os, off_t new_pos)
{
    os_flush(os);
    os->buf.start = new_pos;
    os->m->seek_i(os, new_pos);
}

/**
 * Unsafe alternative to os_write_byte. Only use this method if you know there
 * is no chance of buffer overflow.
 */
#define write_byte(os, b) os->buf.buf[os->buf.pos++] = (frt_uchar)b

/**
 * Write a single byte +b+ to the OutStream +os+
 *
 * @param os the OutStream to write to
 * @param b  the byte to write
 * @raise FRT_IO_ERROR if there is an IO error writing to the filesystem
 */
void os_write_byte(FrtOutStream *os, frt_uchar b)
{
    if (os->buf.pos >= FRT_BUFFER_SIZE) {
        os_flush(os);
    }
    write_byte(os, b);
}

void os_write_bytes(FrtOutStream *os, const frt_uchar *buf, int len)
{
    if (os->buf.pos > 0) {      /* flush buffer */
        os_flush(os);
    }

    if (len < FRT_BUFFER_SIZE) {
        os->m->flush_i(os, buf, len);
        os->buf.start += len;
    }
    else {
        int pos = 0;
        int size;
        while (pos < len) {
            if (len - pos < FRT_BUFFER_SIZE) {
                size = len - pos;
            }
            else {
                size = FRT_BUFFER_SIZE;
            }
            os->m->flush_i(os, buf + pos, size);
            pos += size;
            os->buf.start += size;
        }
    }
}

/**
 * Create a newly allocated and initialized InStream
 *
 * @return a newly allocated and initialized InStream
 */
FrtInStream *is_new()
{
    FrtInStream *is = FRT_ALLOC(FrtInStream);
    is->buf.start = 0;
    is->buf.pos = 0;
    is->buf.len = 0;
    is->ref_cnt_ptr = FRT_ALLOC_AND_ZERO(int);
    return is;
}

/**
 * Refill the InStream's buffer from the store source (filesystem or memory).
 *
 * @param is the InStream to refill
 * @raise FRT_IO_ERROR if there is a error reading from the filesystem
 * @raise FRT_EOF_ERROR if there is an attempt to read past the end of the file
 */
static void is_refill(FrtInStream *is)
{
    off_t start = is->buf.start + is->buf.pos;
    off_t last = start + FRT_BUFFER_SIZE;
    off_t flen = is->m->length_i(is);

    if (last > flen) {          /* don't read past EOF */
        last = flen;
    }

    is->buf.len = last - start;
    if (is->buf.len <= 0) {
        rb_raise(rb_eEOFError, "current pos = %"FRT_OFF_T_PFX"d, "
              "file length = %"FRT_OFF_T_PFX"d", start, flen);
    }

    is->m->read_i(is, is->buf.buf, is->buf.len);

    is->buf.start = start;
    is->buf.pos = 0;
}

/**
 * Unsafe alternative to is_read_byte. Only use this method when you know
 * there is no chance that you will read past the end of the InStream's
 * buffer.
 */
#define read_byte(is) is->buf.buf[is->buf.pos++]

/**
 * Read a singly byte (unsigned char) from the InStream +is+.
 *
 * @param is the Instream to read from
 * @return a single unsigned char read from the InStream +is+
 * @raise FRT_IO_ERROR if there is a error reading from the filesystem
 * @raise FRT_EOF_ERROR if there is an attempt to read past the end of the file
 */
frt_uchar is_read_byte(FrtInStream *is)
{
    if (is->buf.pos >= is->buf.len) {
        is_refill(is);
    }

    return read_byte(is);
}

off_t is_pos(FrtInStream *is)
{
    return is->buf.start + is->buf.pos;
}

frt_uchar *is_read_bytes(FrtInStream *is, frt_uchar *buf, int len)
{
    int i;
    off_t start;

    if ((is->buf.pos + len) < is->buf.len) {
        for (i = 0; i < len; i++) {
            buf[i] = read_byte(is);
        }
    }
    else {                              /* read all-at-once */
        start = is_pos(is);
        is->m->seek_i(is, start);
        is->m->read_i(is, buf, len);

        is->buf.start = start + len;    /* adjust stream variables */
        is->buf.pos = 0;
        is->buf.len = 0;                /* trigger refill on read */
    }
    return buf;
}

void is_seek(FrtInStream *is, off_t pos)
{
    if (pos >= is->buf.start && pos < (is->buf.start + is->buf.len)) {
        is->buf.pos = pos - is->buf.start;  /* seek within buffer */
    }
    else {
        is->buf.start = pos;
        is->buf.pos = 0;
        is->buf.len = 0;                    /* trigger refill() on read() */
        is->m->seek_i(is, pos);
    }
}

void is_close(FrtInStream *is)
{
    if (--(*(is->ref_cnt_ptr)) < 0) {
        is->m->close_i(is);
        free(is->ref_cnt_ptr);
    }
    free(is);
}

FrtInStream *is_clone(FrtInStream *is)
{
    FrtInStream *new_index_i = FRT_ALLOC(FrtInStream);
    memcpy(new_index_i, is, sizeof(FrtInStream));
    (*(new_index_i->ref_cnt_ptr))++;
    return new_index_i;
}

i32 is_read_i32(FrtInStream *is)
{
    return ((i32)is_read_byte(is) << 24) |
        ((i32)is_read_byte(is) << 16) |
        ((i32)is_read_byte(is) << 8) |
        ((i32)is_read_byte(is));
}

i64 is_read_i64(FrtInStream *is)
{
    return ((i64)is_read_byte(is) << 56) |
        ((i64)is_read_byte(is) << 48) |
        ((i64)is_read_byte(is) << 40) |
        ((i64)is_read_byte(is) << 32) |
        ((i64)is_read_byte(is) << 24) |
        ((i64)is_read_byte(is) << 16) |
        ((i64)is_read_byte(is) << 8) |
        ((i64)is_read_byte(is));
}

u32 is_read_u32(FrtInStream *is)
{
    return ((u32)is_read_byte(is) << 24) |
        ((u32)is_read_byte(is) << 16) |
        ((u32)is_read_byte(is) << 8) |
        ((u32)is_read_byte(is));
}

u64 is_read_u64(FrtInStream *is)
{
    return ((u64)is_read_byte(is) << 56) |
        ((u64)is_read_byte(is) << 48) |
        ((u64)is_read_byte(is) << 40) |
        ((u64)is_read_byte(is) << 32) |
        ((u64)is_read_byte(is) << 24) |
        ((u64)is_read_byte(is) << 16) |
        ((u64)is_read_byte(is) << 8) |
        ((u64)is_read_byte(is));
}

/* optimized to use unchecked read_byte if there is definitely space */
unsigned int is_read_vint(FrtInStream *is)
{
    register unsigned int res, b;
    register int shift = 7;

    if (is->buf.pos > (is->buf.len - VINT_MAX_LEN)) {
        b = is_read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = is_read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }
    else {                              /* unchecked optimization */
        b = read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }

    return res;
}

/* optimized to use unchecked read_byte if there is definitely space */
off_t is_read_voff_t(FrtInStream *is)
{
    register off_t res, b;
    register int shift = 7;

    if (is->buf.pos > (is->buf.len - VINT_MAX_LEN)) {
        b = is_read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = is_read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }
    else {                              /* unchecked optimization */
        b = read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }

    return res;
}

/* optimized to use unchecked read_byte if there is definitely space */
u64 is_read_vll(FrtInStream *is)
{
    register u64 res, b;
    register int shift = 7;

    if (is->buf.pos > (is->buf.len - VINT_MAX_LEN)) {
        b = is_read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = is_read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }
    else {                              /* unchecked optimization */
        b = read_byte(is);
        res = b & 0x7F;                 /* 0x7F = 0b01111111 */

        while ((b & 0x80) != 0) {       /* 0x80 = 0b10000000 */
            b = read_byte(is);
            res |= (b & 0x7F) << shift;
            shift += 7;
        }
    }

    return res;
}

void is_skip_vints(FrtInStream *is, register int cnt)
{
    for (; cnt > 0; cnt--) {
        while ((is_read_byte(is) & 0x80) != 0) {
        }
    }
}

/*
 * FIXME: Not used. Do we need/want this?
static void is_read_chars(FrtInStream *is, char *buffer,
                          int off, int len)
{
    int end, i;

    end = off + len;

    for (i = off; i < end; i++) {
        buffer[i] = is_read_byte(is);
    }
}
*/

char *is_read_string(FrtInStream *is)
{
    register int length = (int) is_read_vint(is);
    char *str = FRT_ALLOC_N(char, length + 1);
    str[length] = '\0';

    if (is->buf.pos > (is->buf.len - length)) {
        register int i;
        for (i = 0; i < length; i++) {
            str[i] = is_read_byte(is);
        }
    }
    else {                      /* unchecked optimization */
        memcpy(str, is->buf.buf + is->buf.pos, length);
        is->buf.pos += length;
    }

    return str;
}

char *is_read_string_safe(FrtInStream *is)
{
    register int length = (int) is_read_vint(is);
    char *str = FRT_ALLOC_N(char, length + 1);
    str[length] = '\0';

    FRT_TRY
        if (is->buf.pos > (is->buf.len - length)) {
            register int i;
            for (i = 0; i < length; i++) {
                str[i] = is_read_byte(is);
            }
        }
        else {                      /* unchecked optimization */
            memcpy(str, is->buf.buf + is->buf.pos, length);
            is->buf.pos += length;
        }
    FRT_XCATCHALL
        free(str);
    FRT_XENDTRY

    return str;
}

void os_write_i32(FrtOutStream *os, i32 num)
{
    os_write_byte(os, (frt_uchar)((num >> 24) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 16) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 8) & 0xFF));
    os_write_byte(os, (frt_uchar)(num & 0xFF));
}

void os_write_i64(FrtOutStream *os, i64 num)
{
    os_write_byte(os, (frt_uchar)((num >> 56) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 48) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 40) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 32) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 24) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 16) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 8) & 0xFF));
    os_write_byte(os, (frt_uchar)(num & 0xFF));
}

void os_write_u32(FrtOutStream *os, u32 num)
{
    os_write_byte(os, (frt_uchar)((num >> 24) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 16) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 8) & 0xFF));
    os_write_byte(os, (frt_uchar)(num & 0xFF));
}

void os_write_u64(FrtOutStream *os, u64 num)
{
    os_write_byte(os, (frt_uchar)((num >> 56) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 48) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 40) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 32) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 24) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 16) & 0xFF));
    os_write_byte(os, (frt_uchar)((num >> 8) & 0xFF));
    os_write_byte(os, (frt_uchar)(num & 0xFF));
}

/* optimized to use an unchecked write if there is space */
void os_write_vint(FrtOutStream *os, register unsigned int num)
{
    if (os->buf.pos > VINT_END) {
        while (num > 127) {
            os_write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        os_write_byte(os, (frt_uchar)(num));
    }
    else {
        while (num > 127) {
            write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        write_byte(os, (frt_uchar)(num));
    }
}

/* optimized to use an unchecked write if there is space */
void os_write_voff_t(FrtOutStream *os, register off_t num)
{
    if (os->buf.pos > VINT_END) {
        while (num > 127) {
            os_write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        os_write_byte(os, (frt_uchar)num);
    }
    else {
        while (num > 127) {
            write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        write_byte(os, (frt_uchar)num);
    }
}

/* optimized to use an unchecked write if there is space */
void os_write_vll(FrtOutStream *os, register u64 num)
{
    if (os->buf.pos > VINT_END) {
        while (num > 127) {
            os_write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        os_write_byte(os, (frt_uchar)num);
    }
    else {
        while (num > 127) {
            write_byte(os, (frt_uchar)((num & 0x7f) | 0x80));
            num >>= 7;
        }
        write_byte(os, (frt_uchar)num);
    }
}

void os_write_string_len(FrtOutStream *os, const char *str, int len)
{
    os_write_vint(os, len);
    os_write_bytes(os, (frt_uchar *)str, len);
}
void os_write_string(FrtOutStream *os, const char *str)
{
    os_write_string_len(os, str, (int)strlen(str));
}

/**
 * Determine if the filename is the name of a lock file. Return 1 if it is, 0
 * otherwise.
 *
 * @param filename the name of the file to check
 * @return 1 (true) if the file is a lock file, 0 (false) otherwise
 */
int frt_file_is_lock(const char *filename)
{
    int start = (int) strlen(filename) - 4;
    return ((start > 0) && (strcmp(FRT_LOCK_EXT, &filename[start]) == 0));
}

void is2os_copy_bytes(FrtInStream *is, FrtOutStream *os, int cnt)
{
    int len;
    frt_uchar buf[FRT_BUFFER_SIZE];

    for (; cnt > 0; cnt -= FRT_BUFFER_SIZE) {
        len = ((cnt > FRT_BUFFER_SIZE) ? FRT_BUFFER_SIZE : cnt);
        is_read_bytes(is, buf, len);
        os_write_bytes(os, buf, len);
    }
}

void is2os_copy_vints(FrtInStream *is, FrtOutStream *os, int cnt)
{
    frt_uchar b;
    for (; cnt > 0; cnt--) {
        while (((b = is_read_byte(is)) & 0x80) != 0) {
            os_write_byte(os, b);
        }
        os_write_byte(os, b);
    }
}

/**
 * Test argument used to test the store->each function
 */
struct FileNameListArg
{
    int count;
    int size;
    int total_len;
    char **files;
};

/**
 * Test function used to test store->each function
 */
static void add_file_name(const char *fname, void *arg)
{
    struct FileNameListArg *fnl = (struct FileNameListArg *)arg;
    if (fnl->count >= fnl->size) {
        fnl->size *= 2;
        FRT_REALLOC_N(fnl->files, char *, fnl->size);
    }
    fnl->files[fnl->count++] = frt_estrdup(fname);
    fnl->total_len += strlen(fname) + 2;
}

char *store_to_s(FrtStore *store)
{
    struct FileNameListArg fnl;
    char *buf, *b;
    int i;
    fnl.count = 0;
    fnl.size = 16;
    fnl.total_len = 10;
    fnl.files = FRT_ALLOC_N(char *, 16);

    store->each(store, &add_file_name, &fnl);
    qsort(fnl.files, fnl.count, sizeof(char *), &scmp);
    b = buf = FRT_ALLOC_N(char, fnl.total_len);

    for (i = 0; i < fnl.count; i++) {
        char *fn = fnl.files[i];
        int len = strlen(fn);
        memcpy(b, fn, len);
        b += len;
        *b++ = '\n';
        free(fn);
    }
    *b = '\0';
    free(fnl.files);

    return buf;
}
