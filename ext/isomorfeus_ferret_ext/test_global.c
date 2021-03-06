#include <signal.h>
#include "test.h"
#define _ISOC99_SOURCE
#include <math.h>
#include "fio_tmpfile.h"

/**
 * Test min/max functions
 */
static void test_min_max(TestCase *tc, void *data)
{
    (void)data; /* suppress unused argument warning */

    Aiequal(2, FRT_MAX(2, 1));
    Aiequal(2, FRT_MAX(1, 2));
    Aiequal(2, FRT_MAX(2, 2));
    Aiequal(-2, FRT_MAX(-2, -3));
    Aiequal(-2, FRT_MAX(-3, -2));
    Aiequal(-2, FRT_MAX(-2, -2));

    Aiequal(2, FRT_MIN(3, 2));
    Aiequal(2, FRT_MIN(2, 3));
    Aiequal(2, FRT_MIN(2, 2));
    Aiequal(-2, FRT_MIN(-2, -1));
    Aiequal(-2, FRT_MIN(-1, -2));
    Aiequal(-2, FRT_MIN(-2, -2));

    Aiequal(2, FRT_MAX3(2, 1, 1));
    Aiequal(2, FRT_MAX3(1, 2, 1));
    Aiequal(2, FRT_MAX3(1, 1, 2));
    Aiequal(2, FRT_MAX3(2, 2, 2));
    Aiequal(-2, FRT_MAX3(-2, -3, -3));
    Aiequal(-2, FRT_MAX3(-3, -2, -3));
    Aiequal(-2, FRT_MAX3(-3, -3, -2));
    Aiequal(-2, FRT_MAX3(-2, -2, -2));

    Aiequal(2, FRT_MIN3(2, 3, 3));
    Aiequal(2, FRT_MIN3(3, 2, 3));
    Aiequal(2, FRT_MIN3(3, 3, 2));
    Aiequal(2, FRT_MIN3(2, 2, 2));
    Aiequal(-2, FRT_MIN3(-2, -1, -1));
    Aiequal(-2, FRT_MIN3(-1, -2, -1));
    Aiequal(-2, FRT_MIN3(-1, -1, -2));
    Aiequal(-2, FRT_MIN3(-2, -2, -2));
}

/**
 * Test icmp
 */
static void test_icmp(TestCase *tc, void *data)
{
    (void)data; /* suppress unused argument warning */
    int a = 1, b = 2;
    int array[10] = {7,8,6,5,9,2,3,4,1,0};
    Aiequal( 1, frt_icmp(&b, &a));
    Aiequal(-1, frt_icmp(&a, &b));
    Aiequal( 0, frt_icmp(&a, &a));
    qsort(array, 10, sizeof(int), &frt_icmp);
    Aiequal(0, array[0]);
    Aiequal(3, array[3]);
    Aiequal(6, array[6]);
    Aiequal(9, array[9]);
}

/**
 * Test the various alloc functions
 */
static void test_alloc(TestCase *tc, void *data)
{
    data = frt_lmalloc(100);
    Aiequal(100, *(unsigned long *)data);
    free(data);

    data = frt_u32malloc(100);
    Aiequal(100, *(frt_u32 *)data);
    free(data);

    data = frt_u64malloc(100);
    Aiequal(100, *(frt_u64 *)data);
    free(data);

    data = frt_epstrdup("hello %s%d", 10, "dave", 32);
    Asequal("hello dave32", (char *)data);
    free(data);
}

/**
 * Test the strfmt functions. This method is like sprintf except that it
 * allocates the necessary space for the string and pretty prints floats.
 */
static void test_strfmt(TestCase *tc, void *data)
{
    char *s;
    (void)data; /* suppress unused argument warning */

    s = frt_strfmt("%s%s%s%s", "one", "two", "three", "four");
    Asequal("onetwothreefour", s);
    free(s);
    s = frt_strfmt("%s%s%s%s", "one", "two", "three", "four", "five");
    Asequal("onetwothreefour", s);
    free(s);

    s = frt_strfmt("%d, %d, %d, %d", 987831, 3212346, 1231231, 431231);
    Asequal("987831, 3212346, 1231231, 431231", s);
    free(s);
    s = frt_strfmt("%d, %d, %d, %d", 987831, 3212346, 1231231, 431231, 1209387);
    Asequal("987831, 3212346, 1231231, 431231", s);
    free(s);

    s = frt_strfmt("%f, %f, %f, %f", 0.000234234, 1234.123, 7890.342,
               78907.1200000);
    Asequal("0.000234234, 1234.123, 7890.342, 78907.12", s);
    free(s);

    s = frt_strfmt("%f, %f, %f, %f", 0.000234234, 1234.123, 7890.342, 78907.120,
               1.0);
    Asequal("0.000234234, 1234.123, 7890.342, 78907.12", s);
    free(s);

    s = frt_strfmt("%s, %d, %f, %s, %d, %f", "one", 23462346, 0.0002342,
               "two", 23452346, 0.213451);
    Asequal("one, 23462346, 0.0002342, two, 23452346, 0.213451", s);
    free(s);
    s = frt_strfmt("%s, %d, %f, %s, %d, %f", "one", 23462346, 0.0002342,
               "two", 23452346, 0.213451, "three", 342234, 0.908709);
    Asequal("one, 23462346, 0.0002342, two, 23452346, 0.213451", s);
    free(s);

    s = frt_strfmt("NULL looks like %s", NULL);
    Asequal("NULL looks like (null)", s);
    free(s);
}

/**
 * Test dbl_to_s
 */
static void test_dbl_to_s(TestCase *tc, void *data)
{
    (void)data; /* suppress unused argument warning */
    char buf[100];
    Asequal("4.125123", frt_dbl_to_s(buf, 4.125123l));
    Asequal("1.111111e+06", frt_dbl_to_s(buf, 1111111.2));
    Asequal("Infinity", frt_dbl_to_s(buf, INFINITY));
    Asequal("-Infinity", frt_dbl_to_s(buf, -INFINITY));
    Asequal("NaN", frt_dbl_to_s(buf, NAN));
}

static void test_count_leading_zeros(TestCase *tc, void *data)
{
    (void)data;
    Aiequal(32, frt_count_leading_zeros(0));
    Aiequal(31, frt_count_leading_zeros(1));
    Aiequal(30, frt_count_leading_zeros(2));
    Aiequal(30, frt_count_leading_zeros(3));
    Aiequal( 0, frt_count_leading_zeros(0xffffffff));
}

static void test_count_leading_ones(TestCase *tc, void *data)
{
    (void)data;
    Aiequal( 0, frt_count_leading_ones(0));
    Aiequal( 0, frt_count_leading_ones(1));
    Aiequal( 0, frt_count_leading_ones(2));
    Aiequal( 0, frt_count_leading_ones(3));
    Aiequal(32, frt_count_leading_ones(0xffffffff));
}

static void test_count_trailing_zeros(TestCase *tc, void *data)
{
    (void)data;
    Aiequal(32, frt_count_trailing_zeros(0));
    Aiequal( 0, frt_count_trailing_zeros(1));
    Aiequal( 1, frt_count_trailing_zeros(2));
    Aiequal( 0, frt_count_trailing_zeros(3));
    Aiequal( 4, frt_count_trailing_zeros(0xfffffff0));
    Aiequal( 0, frt_count_trailing_zeros(0xffffffff));
}

static void test_count_trailing_ones(TestCase *tc, void *data)
{
    (void)data;
    Aiequal( 0, frt_count_trailing_ones(0));
    Aiequal( 1, frt_count_trailing_ones(1));
    Aiequal( 0, frt_count_trailing_ones(2));
    Aiequal( 2, frt_count_trailing_ones(3));
    Aiequal( 0, frt_count_trailing_ones(0xfffffff0));
    Aiequal(32, frt_count_trailing_ones(0xffffffff));
}

static void test_count_zeros(TestCase *tc, void *data)
{
    (void)data;
    Aiequal(32, frt_count_zeros(0));
    Aiequal(31, frt_count_zeros(1));
    Aiequal(31, frt_count_zeros(2));
    Aiequal(30, frt_count_zeros(3));
    Aiequal( 4, frt_count_zeros(0xfffffff0));
    Aiequal( 0, frt_count_zeros(0xffffffff));
}

static void test_count_ones(TestCase *tc, void *data)
{
    (void)data;
    Aiequal( 0, frt_count_ones(0));
    Aiequal( 1, frt_count_ones(1));
    Aiequal( 1, frt_count_ones(2));
    Aiequal( 2, frt_count_ones(3));
    Aiequal(28, frt_count_ones(0xfffffff0));
    Aiequal(32, frt_count_ones(0xffffffff));
}

static void test_round2(TestCase *tc, void *data)
{
    (void)data;
    Aiequal(   1, frt_round2(0));
    Aiequal(   2, frt_round2(1));
    Aiequal(   4, frt_round2(2));
    Aiequal(   4, frt_round2(3));
    Aiequal(1024, frt_round2(1023));
    Aiequal(2048, frt_round2(1024));
}

static void test_clean_up(TestCase *tc, void *data)
{
    (void)data;
    int i;
    /* excercise clean_up stack by adding enough objects to overflow stack */
    for (i = 0; i < 100; i++) {
        frt_register_for_cleanup(malloc(4), &free);
    }
    Assert(true, "This will really be tested by valgrind");
}

TestSuite *ts_global(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_min_max, NULL);
    tst_run_test(suite, test_icmp, NULL);
    tst_run_test(suite, test_alloc, NULL);
    tst_run_test(suite, test_strfmt, NULL);
    tst_run_test(suite, test_dbl_to_s, NULL);
    tst_run_test(suite, test_count_leading_zeros, NULL);
    tst_run_test(suite, test_count_leading_ones, NULL);
    tst_run_test(suite, test_count_trailing_zeros, NULL);
    tst_run_test(suite, test_count_trailing_ones, NULL);
    tst_run_test(suite, test_count_zeros, NULL);
    tst_run_test(suite, test_count_ones, NULL);
    tst_run_test(suite, test_round2, NULL);
    tst_run_test(suite, test_clean_up, NULL);

    return suite;
}
