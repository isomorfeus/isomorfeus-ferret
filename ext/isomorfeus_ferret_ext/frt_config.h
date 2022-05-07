#ifndef FRT_DEFINES_H
#define FRT_DEFINES_H

#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <limits.h>
#include "frt_posh.h"

#ifndef false
#define false 0
#endif
#ifndef true
#define true  1
#endif

#ifndef bool
typedef unsigned int  bool;
#endif
typedef unsigned char frt_uchar;

typedef posh_u16_t frt_u16;
typedef posh_i16_t frt_i16;
typedef posh_u32_t frt_u32;
typedef posh_i32_t frt_i32;
typedef posh_u64_t frt_u64;
typedef posh_i64_t frt_i64;

#if defined POSH_OS_WIN64
typedef off64_t frt_off_t;
#else
typedef off_t frt_off_t;
#endif

#if ( LONG_MAX == 2147483647 ) && defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
#define FRT_OFF_T_PFX "ll"
#else
#define FRT_OFF_T_PFX "l"
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && !defined(__cplusplus)
#define FRT_IS_C99
#define FRT_HAS_ISO_VARARGS
#define FRT_HAS_VARARGS
#endif

#if defined(__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__cplusplus)
#define FRT_HAS_GNUC_VARARGS
#define FRT_HAS_VARARGS
#endif

#endif
