#ifndef FRT_T_HELPER_H
#define FRT_T_HELPER_H

#include "frt_global.h"

#define TEST_WORD_LIST_SIZE 1930
#define TEST_WORD_LIST_MAX_LEN 23

#if defined POSH_OS_WIN32 || defined POSH_OS_WIN64
#define TEST_DIR ".\\test\\testdir\\store"
#else
#define TEST_DIR "./test/testdir/store"
#endif

extern const char *test_word_list[];

extern char *make_random_string(char *buf, int num_words);
extern char *get_nth_word(char *words, char *buf, int n, int *s, int *e);
extern int nth_word_eql(char *words, char *word, int n);
extern int s2l(const char *str, int *arr);
extern bool frt_ary_includes(int *array, int size, int val);
extern char *num_to_str(int num);
extern FILE *temp_open();

#endif
