#ifndef FRT_THREADING_H
#define FRT_THREADING_H

#include <pthread.h>

typedef struct FrtHash *frt_thread_key_t;

// extern void frb_thread_once(int *once_control, void (*init_routine) (void));
extern int frb_thread_key_create(frt_thread_key_t *key, void (*destr_function)(void *));
extern void frb_thread_key_delete(frt_thread_key_t key);
extern void frb_thread_setspecific(frt_thread_key_t key, const void *pointer);
extern void *frb_thread_getspecific(frt_thread_key_t key);

#define frt_thread_key_create(a, b) frb_thread_key_create(a, b)
#define frt_thread_key_delete(a) frb_thread_key_delete(a)
#define frt_thread_setspecific(a, b) frb_thread_setspecific(a, b)
#define frt_thread_getspecific(a) frb_thread_getspecific(a)
#define frt_thread_once(a, b) pthread_once(a, b)

#endif
