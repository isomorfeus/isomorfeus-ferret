#ifndef __FERRET_H_
#define __FERRET_H_
#include "frt_global.h"
#include "frt_hashset.h"
#include "frt_document.h"
#include <ruby.h>

/* IDs */
extern ID id_new;
extern ID id_call;
extern ID id_hash;
extern ID id_eql;
extern ID id_capacity;
extern ID id_less_than;
extern ID id_lt;
extern ID id_is_directory;
extern ID id_close;
extern ID id_cclass;
extern ID id_data;

/* Symbols */
extern VALUE sym_yes;
extern VALUE sym_no;
extern VALUE sym_true;
extern VALUE sym_false;
extern VALUE sym_path;
extern VALUE sym_dir;

/* Modules */
extern VALUE mFerret;
extern VALUE mIndex;
extern VALUE mSearch;
extern VALUE mStore;
extern VALUE mStringHelper;
extern VALUE mSpans;

/* Classes */
extern VALUE cDirectory;
extern VALUE cFileNotFoundError;
extern VALUE cLockError;
extern VALUE cTerm;

/* Ferret Inits */
extern void Init_Utils();
extern void Init_Analysis();
extern void Init_Store();
extern void Init_Index();
extern void Init_Search();
extern void Init_QueryParser();

extern void frb_raise(int excode, const char *msg);
extern void frb_create_dir(VALUE rpath);
extern VALUE frb_hs_to_rb_ary(FrtHashSet *hs);
extern void *frb_rb_data_ptr(VALUE val);
extern ID frb_field(VALUE rfield);
extern VALUE frb_get_term(ID field, const char *term);
extern char *json_concat_string(char *s, char *field);
extern char *rs2s(VALUE rstr);
extern char *rstrdup(VALUE rstr);

#endif

#define frb_mark_cclass(klass) rb_ivar_set(klass, id_cclass, Qtrue)
#define frb_is_cclass(obj) (rb_ivar_defined(CLASS_OF(obj), id_cclass))
