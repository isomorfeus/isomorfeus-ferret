#include "frb_lang.h"
#include "frt_internal.h"

struct timeval rb_time_interval _((VALUE));
void micro_sleep(const int micro_seconds) {
    rb_thread_wait_for(rb_time_interval(rb_float_new((double)micro_seconds/1000000.0)));
}
