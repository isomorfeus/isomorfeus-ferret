###
# Ferret Makefile
#
# Common CFLAGS options
#  -D_FILE_OFFSET_BITS=64     // set this option for large-file support
#  -g                         // set -g for debugging purposes
#  -pg                        // use this option if you want to use gprof
#  -DDEBUG                    // some error checking only occurs in DEBUG mode
#  -DHAVE_GDB                 // defined on platforms that have gdb
#  -D_POSIX_C_SOURCE=2        // for popen which is used to print stacktraces
#  -fprofile-arcs -ftest-coverage // for gcov
###

srcdir = ../../ext/isomorfeus_ferret_ext

CC       = gcc
CINCS    = -I$(srcdir) -Iruby_include
DEFS     = -D_FILE_OFFSET_BITS=64 -DDEBUG -D_POSIX_C_SOURCE=2 -DWITHOUT_RUBY
DEFS    += -DHAVE_GDB
CFLAGS   = -O2 -std=c99 -pedantic -Wall -Wextra $(CINCS) -g -fno-common $(DEFS)
LDFLAGS  = -lm -lpthread
DEP_DIR  = .deps
GCOV_DIR = .gcov
VPATH    = test:src:benchmark

###
# Objects
###

OBJS = \
frt_analysis.o          frt_array.o              frt_bitvector.o        frt_compound_io.o     \
frt_document.o          frt_except.o             frt_filter.o           frt_fs_store.o        \
frt_global.o            frt_hash.o               frt_hashset.o          frt_helper.o          \
frt_ind.o               frt_index.o              frt_mempool.o          frt_multimapper.o     \
frt_posh.o              frt_priorityqueue.o      frt_q_boolean.o        frt_q_const_score.o   \
frt_q_filtered_query.o  frt_q_fuzzy.o            frt_q_match_all.o      frt_q_multi_term.o    \
frt_q_parser.o          frt_q_phrase.o           frt_q_prefix.o         frt_q_range.o         \
frt_q_span.o            frt_q_term.o             frt_q_wildcard.o       frt_ram_store.o       \
frt_search.o            frt_similarity.o         frt_sort.o             frt_stopwords.o       \
frt_store.o             frt_term_vectors.o       frt_field_index.o      frt_lang.o            \
frt_scanner.o           frt_scanner_mb.o         frt_symbol.o           frt_scanner_utf8.o

BENCH_OBJS = benchmark.o bm_bitvector.o bm_hash.c bm_micro_string.c bm_store.c

TEST_OBJS = \
test_multimapper.o      test_q_const_score.o     test_threading.o       test_q_fuzzy.o        \
test_term.o             test_q_filtered.o        testhelper.o           test_hash.o           \
test_fs_store.o         test_segments.o          test_similarity.o      test_except.o         \
test_store.o            test_highlighter.o       test_hashset.o         test_global.o         \
test_bitvector.o        test_search.o            test_fields.o          test_index.o          \
test_mempool.o          test_compound_io.o       test_q_parser.o        test_term_vectors.o   \
test_array.o            test_document.o          test_helper.o          test_test.o           \
test.o                  test_q_span.o            test_analysis.o        test_filter.o         \
test_priorityqueue.o    test_sort.o              test_ram_store.o       test_file_deleter.o   \
test_lang.o             test_symbol.o            test_1710.o

STEMMER_OBJS = \
libstemmer.o                 stem_api.o                    stem_utilities.o             \
stem_ISO_8859_1_danish.o     stem_ISO_8859_1_dutch.o       stem_ISO_8859_1_english.o    \
stem_ISO_8859_1_finnish.o    stem_ISO_8859_1_french.o      stem_ISO_8859_1_german.o     \
stem_ISO_8859_1_hungarian.o  stem_ISO_8859_1_italian.o     stem_ISO_8859_1_norwegian.o  \
stem_ISO_8859_1_porter.o     stem_ISO_8859_1_portuguese.o  stem_ISO_8859_1_spanish.o    \
stem_ISO_8859_1_swedish.o    stem_ISO_8859_2_romanian.o    stem_KOI8_R_russian.o        \
stem_UTF_8_danish.o          stem_UTF_8_dutch.o            stem_UTF_8_english.o         \
stem_UTF_8_finnish.o         stem_UTF_8_french.o           stem_UTF_8_german.o          \
stem_UTF_8_hungarian.o       stem_UTF_8_italian.o          stem_UTF_8_norwegian.o       \
stem_UTF_8_porter.o          stem_UTF_8_portuguese.o       stem_UTF_8_romanian.o        \
stem_UTF_8_russian.o         stem_UTF_8_spanish.o          stem_UTF_8_swedish.o         \
stem_UTF_8_turkish.o

###
# Common Targets
###

test: testall
	@rm -f *.gcda
	@./testall -q -v

testall: $(TEST_OBJS) $(OBJS) $(STEMMER_OBJS)
	@echo Building task: $@ ...
	@$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

benchall: $(BENCH_OBJS) $(OBJS) $(STEMMER_OBJS)
	@echo Building task: $@ ...
	@$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

bench: benchall
	@./benchall

clean:
	@rm -f  $(BENCH_OBJS) $(OBJS) $(STEMMER_OBJS) $(TEST_OBJS)
	@rm -f  testall gmon.out
	@rm -rf $(DEP_DIR) $(GCOV_DIR)

###
# Profiling + coverage
# Note: Make clean before using these for the first time.
###

gcov: CFLAGS += -fprofile-arcs -ftest-coverage
gcov: test $(OBJS:%.o=%.c.gcov)
	@head -1 $(OBJS:%.o=$(GCOV_DIR)/%.c.gcov.out) \
             | grep -v '==>' | egrep -v '^[ ]*$$' | sort -rn -k 2
	@rm -f test*gcda test*gcno

gprof: CFLAGS += -pg
gprof: test

valgrind: testall
	valgrind --suppressions=valgrind.supp \
                 --leak-check=yes             \
                 --show-reachable=yes         \
                 --workaround-gcc296-bugs=yes -v ./testall -q

gen_valsupp: testall
	valgrind --suppressions=valgrind.supp \
                 --leak-check=yes             \
                 --show-reachable=yes         \
                 --gen-suppressions=yes       \
                 --workaround-gcc296-bugs=yes -v ./testall -q

###
# Dependency build
# Ref: http://make.paulandlesley.org/autodep.html
###



frt_%.o : $(srcdir)/frt_%.c
	@echo Compiling: $< ...
	@$(COMPILE.c) -MD -o $@ $<

libstemmer.o : $(srcdir)/libstemmer.c
	@echo Compiling: $< ...
	@$(COMPILE.c) -MD -o $@ $<

stem_%.o : $(srcdir)/stem_%.c
	@echo Compiling: $< ...
	@$(COMPILE.c) -MD -o $@ $<

%.o : %.c
	@mkdir -p `dirname $(DEP_DIR)/$*.P`
	@echo Compiling: $< ...
	@$(COMPILE.c) -MD -o $@ $<
	@cp $*.d $(DEP_DIR)/$*.P; \
            sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
                -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $(DEP_DIR)/$*.P; \
            rm -f $*.d

# src/q_parser.c: src/q_parser.y

# %.c : %.rl
#	ragel $< -o $@

# %.c : %.y
#	yacc -o $@ -p frt_ $<

###
# Generate gcov stats
###

%.c.gcov : %.o
	@mkdir -p $(GCOV_DIR)
	@if [ -e $(*F).gcda ]; then mv $(*F).gcda $(GCOV_DIR); fi
	@if [ -e $(*F).gcno ]; then mv $(*F).gcno $(GCOV_DIR); fi
	@echo Generating coverage for: $< ...
	@-if [ -e $< ] ; then \
             echo -n $<: " " > $(GCOV_DIR)/$@.out ;                          \
             gcov -o $(GCOV_DIR) $<                                          \
                 | head -2         | tail -1                                 \
                 | cut -d ':' -f 2 | cut -d '%' -f 1 >> $(GCOV_DIR)/$@.out ; \
             if [ -e $@ ] ; then mv $@ $(GCOV_DIR) ; fi ;                    \
         fi

###
# Etc
###

.PHONY: clean all test bench gcov gprof valgrind gen_valsupp
