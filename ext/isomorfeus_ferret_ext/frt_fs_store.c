#include "frt_store.h"
#include "frt_lang.h"
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64) && !defined __MINGW32__
# include <io.h>
# include "frt_win32.h"
# ifndef sleep
#   define sleep _sleep
# endif
# ifndef DIR_SEPARATOR
#   define DIR_SEPARATOR "\\"
#   define DIR_SEPARATOR_CHAR '\\'
# endif
# ifndef S_IRUSR
#   define S_IRUSR _S_IREAD
# endif
# ifndef S_IWUSR
#   define S_IWUSR _S_IWRITE
# endif
#else
# define DIR_SEPARATOR "/"
# define DIR_SEPARATOR_CHAR '/'
# include <unistd.h>
# include <dirent.h>
#endif
#ifndef O_BINARY
# define O_BINARY 0
#endif

extern void frt_micro_sleep(const int micro_seconds);

/**
 * Create a filepath for a file in the store using the operating systems
 * default file seperator.
 */
static char *join_path(char *buf, const char *base, const char *filename)
{
  snprintf(buf, FRT_MAX_FILE_PATH, "%s"DIR_SEPARATOR"%s", base, filename);
  return buf;
}

static void fs_touch(FrtStore *store, const char *filename)
{
    int f;
    char path[FRT_MAX_FILE_PATH];
    join_path(path, store->dir.path, filename);
    if ((f = creat(path, store->file_mode)) == 0) {
        rb_raise(rb_eIOError, "fs_touch: couldn't create file %s: <%s>", path,
              strerror(errno));
    }
    close(f);
}

static int fs_exists(FrtStore *store, const char *filename)
{
    int fd;
    char path[FRT_MAX_FILE_PATH];
    join_path(path, store->dir.path, filename);
    fd = open(path, 0);
    if (fd < 0) {
        if (errno != ENOENT) {
            rb_raise(rb_eIOError, "fs_exists: checking existance of %s: <%s>", path,
                  strerror(errno));
        }
        return false;
    }
    close(fd);
    return true;
}

static int fs_remove(FrtStore *store, const char *filename)
{
    char path[FRT_MAX_FILE_PATH];
    return remove(join_path(path, store->dir.path, filename));
}

static void fs_rename(FrtStore *store, const char *from, const char *to)
{
    char path1[FRT_MAX_FILE_PATH], path2[FRT_MAX_FILE_PATH];

#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
    remove(join_path(path1, store->dir.path, to));
#endif

    if (rename(join_path(path1, store->dir.path, from),
               join_path(path2, store->dir.path, to)) < 0) {
        rb_raise(rb_eIOError, "fs_rename: couldn't rename file \"%s\" to \"%s\": <%s>",
              path1, path2, strerror(errno));
    }
}

static int fs_count(FrtStore *store)
{
    int cnt = 0;
    struct dirent *de;
    DIR *d = opendir(store->dir.path);

    if (!d) {
        rb_raise(rb_eIOError, "fs_count: counting files in %s: <%s>",
              store->dir.path, strerror(errno));
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] > '/') { /* skip ., .., / and '\0'*/
            cnt++;
        }
    }
    closedir(d);

    return cnt;
}

static void fs_each(FrtStore *store, void (*func)(const char *fname, void *arg), void *arg)
{
    struct dirent *de;
    DIR *d = opendir(store->dir.path);

    if (!d) {
        rb_raise(rb_eIOError, "fs_each: in %s: <%s>",
              store->dir.path, strerror(errno));
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] > '/' /* skip ., .., / and '\0'*/
                && !frt_file_is_lock(de->d_name)) {
            func(de->d_name, arg);
        }
    }
    closedir(d);
}

static void fs_clear_locks(FrtStore *store)
{
    struct dirent *de;
    DIR *d = opendir(store->dir.path);

    if (!d) {
        rb_raise(rb_eIOError, "fs_clear_locks: in %s: <%s>",
              store->dir.path, strerror(errno));
    }

    while ((de = readdir(d)) != NULL) {
        if (frt_file_is_lock(de->d_name)) {
            char path[FRT_MAX_FILE_PATH];
            remove(join_path(path, store->dir.path, de->d_name));
        }
    }
    closedir(d);
}

static void remove_if_index_file(const char *base_path, const char *file_name)
{
    char path[FRT_MAX_FILE_PATH];
    char *basename;
    join_path(path, base_path, file_name);
    /* get basename of path */
    basename = strrchr(path, DIR_SEPARATOR_CHAR);
    basename = (basename ? basename + 1 : path);
    /* we don't want to delete non-index files here */
    if (frt_file_name_filter_is_index_file(basename, true)) {
        remove(path);
    }
}

static void fs_clear(FrtStore *store)
{
    struct dirent *de;
    DIR *d = opendir(store->dir.path);

    if (!d) {
        rb_raise(rb_eIOError, "fs_clear: in %s: <%s>",
              store->dir.path, strerror(errno));
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] > '/' /* skip ., .., / and '\0'*/
                && !frt_file_is_lock(de->d_name)) {
            remove_if_index_file(store->dir.path, de->d_name);
        }
    }
    closedir(d);
}

/**
 * Clear all files which belong to the index. Use fs_clear to clear the
 * directory regardless of the files origin.
 */
static void fs_clear_all(FrtStore *store)
{
    struct dirent *de;
    DIR *d = opendir(store->dir.path);

    if (!d) {
        rb_raise(rb_eIOError, "fs_clear_all: in %s: <%s>",
              store->dir.path, strerror(errno));
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] > '/') { /* skip ., .., / and '\0'*/
            remove_if_index_file(store->dir.path, de->d_name);
        }
    }
    closedir(d);
}

/**
 * Destroy the store.
 *
 * @param p the store to destroy
 * @rb_raise FRT_IO_ERROR if there is an error deleting the locks
 */
static void fs_destroy(FrtStore *store)
{
    FRT_TRY
        fs_clear_locks(store);
    FRT_XCATCHALL
        FRT_HANDLED();
    FRT_XENDTRY
    free(store->dir.path);
}

static frt_off_t fs_length(FrtStore *store, const char *filename)
{
    char path[FRT_MAX_FILE_PATH];
    struct stat stt;

    if (stat(join_path(path, store->dir.path, filename), &stt)) {
        rb_raise(rb_eIOError, "fs_length: %s: <%s>", path,
              strerror(errno));
    }

    return stt.st_size;
}

static void fso_flush_i(FrtOutStream *os, const frt_uchar *src, int len)
{
    if (len == 0) { return; }
    if (len != write(os->file.fd, src, len)) {
        rb_raise(rb_eIOError, "fso_flush: of length %d, <%s>", len,
              strerror(errno));
    }
}

static void fso_seek_i(FrtOutStream *os, frt_off_t pos) {
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64)
    if (_lseeki64(os->file.fd, pos, SEEK_SET) < 0) {
        rb_raise(rb_eIOError, "fso_seek_i: seeking position %"FRT_OFF_T_PFX"d: <%s>",
            pos, strerror(errno));
    }
#else
    if (lseek(os->file.fd, pos, SEEK_SET) < 0) {
        rb_raise(rb_eIOError, "fso_seek_i: seeking position %"FRT_OFF_T_PFX"d: <%s>",
            pos, strerror(errno));
    }
#endif
}

static void fso_close_i(FrtOutStream *os) {
    if (close(os->file.fd)) {
        rb_raise(rb_eIOError, "fso_close_i: <%s>", strerror(errno));
    }
}

static const struct FrtOutStreamMethods FS_OUT_STREAM_METHODS = {
    fso_flush_i,
    fso_seek_i,
    fso_close_i
};

static FrtOutStream *fs_new_output(FrtStore *store, const char *filename) {
    char path[FRT_MAX_FILE_PATH];
    int fd = open(join_path(path, store->dir.path, filename), O_WRONLY | O_CREAT | O_BINARY, store->file_mode);
    if (fd < 0) {
        rb_raise(rb_eIOError, "fs_new_output: couldn't create OutStream %s: <%s>",
              path, strerror(errno));
    }

    FrtOutStream *os = frt_os_new();
    os->file.fd = fd;
    os->m = &FS_OUT_STREAM_METHODS;
    return os;
}

static void fsi_read_i(FrtInStream *is, frt_uchar *path, int len)
{
    int fd = is->f->file.fd;
    frt_off_t pos = frt_is_pos(is);
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64)
    if (pos != _lseeki64(fd, 0, SEEK_CUR)) {
        _lseeki64(fd, pos, SEEK_SET);
    }
#else
    if (pos != lseek(fd, 0, SEEK_CUR)) {
        lseek(fd, pos, SEEK_SET);
    }
#endif
    if (read(fd, path, len) != len) {
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64)
        /* win: the wrong value can be returned for some reason so double check */
        if (_lseeki64(fd, 0, SEEK_CUR) != (pos + len)) {
            rb_raise(rb_eIOError, "fsi_read_i: couldn't read %d chars from %s: <%s>",
                len, path, strerror(errno));
        }
#else
        rb_raise(rb_eIOError, "fsi_read_i: couldn't read %d chars from %s: <%s>",
            len, path, strerror(errno));
#endif
    }
}

static void fsi_seek_i(FrtInStream *is, frt_off_t pos) {
#if (defined POSH_OS_WIN32 || defined POSH_OS_WIN64)
    if (_lseeki64(is->f->file.fd, pos, SEEK_SET) < 0) {
        rb_raise(rb_eIOError, "fsi_seek_i: seeking pos %"FRT_OFF_T_PFX"d: <%s>",
            pos, strerror(errno));
    }
#else
    if (lseek(is->f->file.fd, pos, SEEK_SET) < 0) {
        rb_raise(rb_eIOError, "fsi_seek_i: seeking pos %"FRT_OFF_T_PFX"d: <%s>",
            pos, strerror(errno));
    }
#endif
}

static void fsi_close_i(FrtInStream *is)
{
    if (close(is->f->file.fd)) {
        rb_raise(rb_eIOError, "fsi_close_i: <%s>", strerror(errno));
    }
    if (is->d.path) free(is->d.path);
}

static frt_off_t fsi_length_i(FrtInStream *is)
{
    struct stat stt;
    if (fstat(is->f->file.fd, &stt)) {
        rb_raise(rb_eIOError, "fsi_length_i: fstat failed: <%s>", strerror(errno));
    }
    return stt.st_size;
}

static const struct FrtInStreamMethods FS_IN_STREAM_METHODS = {
    fsi_read_i,
    fsi_seek_i,
    fsi_length_i,
    fsi_close_i
};

static FrtInStream *fs_open_input(FrtStore *store, const char *filename)
{
    FrtInStream *is;
    char path[FRT_MAX_FILE_PATH];
    int fd = open(join_path(path, store->dir.path, filename), O_RDONLY | O_BINARY);
    if (fd < 0) {
        FRT_RAISE(FRT_FILE_NOT_FOUND_ERROR,
              "fs_open_input: tried to open \"%s\", but it doesn't exist: <%s> ",
              path, strerror(errno));
    }
    is = frt_is_new();
    is->f->file.fd = fd;
    is->f->ref_cnt = 1;
    is->d.path = frt_estrdup(path);
    is->m = &FS_IN_STREAM_METHODS;
    return is;
}

#define LOCK_OBTAIN_TIMEOUT 50

static int fs_lock_obtain(FrtLock *lock)
{
    int f;
    int trys = LOCK_OBTAIN_TIMEOUT;
    while (((f =
             open(lock->name, O_CREAT | O_EXCL | O_RDWR,
                   S_IRUSR | S_IWUSR)) < 0) && (trys > 0)) {

        /* sleep for 10 milliseconds */
        frt_micro_sleep(10000);
        trys--;
    }
    if (f >= 0) {
        close(f);
        return true;
    }
    else {
        return false;
    }
}

static int fs_lock_is_locked(FrtLock *lock)
{
    int f = open(lock->name, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (f >= 0) {
        if (close(f) || remove(lock->name)) {
            rb_raise(rb_eIOError, "fs_lock_is_locked: couldn't close lock \"%s\": <%s>", lock->name,
                  strerror(errno));
        }
        return false;
    }
    else {
        return true;
    }
}

static void fs_lock_release(FrtLock *lock)
{
    remove(lock->name);
}

static FrtLock *fs_open_lock_i(FrtStore *store, const char *lockname)
{
    FrtLock *lock = FRT_ALLOC(FrtLock);
    char lname[100];
    char path[FRT_MAX_FILE_PATH];
    snprintf(lname, 100, "%s%s.lck", FRT_LOCK_PREFIX, lockname);
    lock->name = frt_estrdup(join_path(path, store->dir.path, lname));
    lock->store = store;
    FRT_REF(store);
    lock->obtain = &fs_lock_obtain;
    lock->release = &fs_lock_release;
    lock->is_locked = &fs_lock_is_locked;
    lock->rlock = Qnil;
    return lock;
}

static void fs_close_lock_i(FrtLock *lock)
{
    frt_store_close(lock->store);
    remove(lock->name);
    free(lock->name);
    free(lock);
}

static FrtHash *stores = NULL;

static pthread_mutex_t stores_mutex = PTHREAD_MUTEX_INITIALIZER;

static void fs_close_i(FrtStore *store)
{
    pthread_mutex_lock(&stores_mutex);
    frt_h_del(stores, store->dir.path);
    pthread_mutex_unlock(&stores_mutex);
}

static FrtStore *fs_store_new(const char *pathname)
{
    FrtStore *new_store = frt_store_new();

    new_store->file_mode = S_IRUSR | S_IWUSR;
#if !defined POSH_OS_WIN32 && !defined POSH_OS_WIN64
    struct stat stt;
    if (!stat(pathname, &stt)) {
        gid_t st_gid = stt.st_gid;
        bool is_grp = (st_gid == getgid());

        if (!is_grp) {
            int size = getgroups(0, NULL);
            gid_t list[size];

            if (getgroups(size, list) != -1) {
                int i = 0;
                while (i < size && !(is_grp = (st_gid == list[i]))) i++;
            }
        }

        if (is_grp) {
            if (stt.st_mode & S_IWGRP) {
                umask(S_IWOTH);
            }
            new_store->file_mode |= stt.st_mode & (S_IRGRP | S_IWGRP);
        }
    }
#endif

    new_store->dir.path      = frt_estrdup(pathname);
    new_store->touch         = &fs_touch;
    new_store->exists        = &fs_exists;
    new_store->remove        = &fs_remove;
    new_store->rename        = &fs_rename;
    new_store->count         = &fs_count;
    new_store->close_i       = &fs_close_i;
    new_store->clear         = &fs_clear;
    new_store->clear_all     = &fs_clear_all;
    new_store->clear_locks   = &fs_clear_locks;
    new_store->length        = &fs_length;
    new_store->each          = &fs_each;
    new_store->new_output    = &fs_new_output;
    new_store->open_input    = &fs_open_input;
    new_store->open_lock_i   = &fs_open_lock_i;
    new_store->close_lock_i  = &fs_close_lock_i;
    return new_store;
}

FrtStore *frt_open_fs_store(const char *pathname) {
    FrtStore *store = NULL;

    if (!stores) {
        stores = frt_h_new_str(NULL, (frt_free_ft)fs_destroy);
        frt_register_for_cleanup(stores, (frt_free_ft)frt_h_destroy);
    }

    pthread_mutex_lock(&stores_mutex);
    store = (FrtStore *)frt_h_get(stores, pathname);
    if (store) {
        FRT_REF(store);
    } else {
        store = fs_store_new(pathname);
        frt_h_set(stores, store->dir.path, store);
    }
    pthread_mutex_unlock(&stores_mutex);

    return store;
}
