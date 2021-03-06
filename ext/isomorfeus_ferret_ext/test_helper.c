#include "frt_helper.h"
#include "test.h"

static void test_hlp_string_diff(TestCase *tc, void *data)
{
    (void)data; /* suppress unused argument warning */

    Aiequal(3, frt_hlp_string_diff("David", "Dave"));
    Aiequal(0, frt_hlp_string_diff("David", "Erik"));
    Aiequal(4, frt_hlp_string_diff("book", "bookworm"));
    Aiequal(4, frt_hlp_string_diff("bookstop", "book"));
    Aiequal(4, frt_hlp_string_diff("bookstop", "bookworm"));
}

void test_byte2float(TestCase *tc, void *data)
{
    int i;
    (void)data;

    for (i = 0; i < 256; i++) {
        Aiequal(i, frt_float2byte(frt_byte2float((char)i)));
    }
}

void test_int2float(TestCase *tc, void *data)
{
    int i;
    (void)data;

    for (i = 0; i < 256; i++) {
        int x = rand() % 20000000;
        float f = (float)i;
        Aiequal(x, frt_float2int(frt_int2float(x)));
        Afequal(f, frt_int2float(frt_float2int(f)));
    }
}

TestSuite *ts_helper(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_hlp_string_diff, NULL);
    tst_run_test(suite, test_byte2float, NULL);
    tst_run_test(suite, test_int2float, NULL);

    return suite;
}
