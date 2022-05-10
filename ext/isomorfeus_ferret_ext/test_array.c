#include "frt_array.h"
#include <string.h>
#include "test.h"

static void frt_ary_free_mock(void *p)
{
    char *str = (char *)p;
    strcpy(str, "free");
}

void test_ary_destroy(TestCase *tc, void *data)
{
    void **ary = frt_ary_new();
    char str1[10] = "alloc1";
    char str2[10] = "alloc2";
    (void)data;

    frt_ary_set(ary, 0, str1);
    Aiequal(1, frt_ary_sz(ary));
    Asequal("alloc1", ary[0]);
    frt_ary_set(ary, 0, str2);
    Asequal("alloc2", ary[0]);
    frt_ary_push(ary, str1);
    Aiequal(2, frt_ary_sz(ary));
    Asequal("alloc1", ary[1]);
    frt_ary_delete(ary, 0, &frt_ary_free_mock);
    Aiequal(1, frt_ary_sz(ary));
    Asequal("free", str2);
    frt_ary_destroy(ary, &frt_ary_free_mock);
    Asequal("free", str1);
}

#define ARY_STRESS_SIZE 1000
void stress_ary(TestCase *tc, void *data)
{
    int i;
    char buf[100], *t;
    void **ary = frt_ary_new();
    (void)data;

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        frt_ary_push(ary, frt_estrdup(buf));
    }

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        sprintf(buf, "<%d>", i);
        t = (char *)frt_ary_shift(ary);
        Asequal(buf, t);
        free(t);
    }

    Aiequal(0, frt_ary_sz(ary));

    for (i = 0; i < ARY_STRESS_SIZE; i++) {
        Apnull(ary[i]);
    }
    frt_ary_destroy(ary, &free);
}

struct TestPoint {
    int x;
    int y;
};

#define tp_ary_set(ary, i, x_val, y_val) do {\
    frt_ary_resize(ary, i);\
    ary[i].x = x_val;\
    ary[i].y = y_val;\
} while (0)

void test_typed_ary(TestCase *tc, void *data)
{
    struct TestPoint *points = frt_ary_new_type_capa(struct TestPoint, 5);
    (void)data;

    Aiequal(5, frt_ary_capa(points));
    Aiequal(0, frt_ary_sz(points));
    Aiequal(sizeof(struct TestPoint), frt_ary_type_size(points));

    tp_ary_set(points, 0, 1, 2);
    Aiequal(5, frt_ary_capa(points));
    Aiequal(1, frt_ary_sz(points));
    Aiequal(sizeof(struct TestPoint), frt_ary_type_size(points));
    Aiequal(1, points[0].x);
    Aiequal(2, points[0].y);

    tp_ary_set(points, 5, 15, 20);
    Aiequal(6, frt_ary_size(points));
    Aiequal(15, points[5].x);
    Aiequal(20, points[5].y);

    tp_ary_set(points, 1, 1, 1);
    tp_ary_set(points, 2, 2, 2);
    tp_ary_set(points, 3, 3, 3);
    tp_ary_set(points, 4, 4, 4);

    Aiequal(6, frt_ary_size(points));
    Aiequal(1, points[0].x);
    Aiequal(2, points[0].y);
    Aiequal(1, points[1].x);
    Aiequal(1, points[1].y);
    Aiequal(2, points[2].x);
    Aiequal(2, points[2].y);
    Aiequal(3, points[3].x);
    Aiequal(3, points[3].y);
    Aiequal(4, points[4].x);
    Aiequal(4, points[4].y);
    Aiequal(15, points[5].x);
    Aiequal(20, points[5].y);
    frt_ary_free(points);
}

TestSuite *ts_array(TestSuite *suite)
{
    suite = ADD_SUITE(suite);

    tst_run_test(suite, test_ary_destroy, NULL);
    tst_run_test(suite, stress_ary, NULL);
    tst_run_test(suite, test_typed_ary, NULL);

    return suite;
}
