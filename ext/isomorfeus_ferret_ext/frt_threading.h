#ifndef FRT_THREADING_H
#define FRT_THREADING_H

#include <pthread.h>

typedef pthread_mutex_t frt_mutex_t;
typedef struct FrtHash *frt_thread_key_t;
typedef pthread_once_t frt_thread_once_t;

// extern void frb_thread_once(int *once_control, void (*init_routine) (void));
extern int frb_thread_key_create(frt_thread_key_t *key, void (*destr_function)(void *));
extern void frb_thread_key_delete(frt_thread_key_t key);
extern void frb_thread_setspecific(frt_thread_key_t key, const void *pointer);
extern void *frb_thread_getspecific(frt_thread_key_t key);

#define FRT_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define FRT_THREAD_ONCE_INIT PTHREAD_ONCE_INIT
#define frt_mutex_init(a, b) pthread_mutex_init(a, b)
#define frt_mutex_lock(a) pthread_mutex_lock(a)
#define frt_mutex_trylock(a) pthread_mutex_trylock(a)
#define frt_mutex_unlock(a) pthread_mutex_unlock(a)
#define frt_mutex_destroy(a) pthread_mutex_destroy(a)
#define frt_thread_key_create(a, b) frb_thread_key_create(a, b)
#define frt_thread_key_delete(a) frb_thread_key_delete(a)
#define frt_thread_setspecific(a, b) frb_thread_setspecific(a, b)
#define frt_thread_getspecific(a) frb_thread_getspecific(a)
#define frt_thread_once(a, b) pthread_once(a, b)

#endif
