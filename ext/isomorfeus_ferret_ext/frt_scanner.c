
#line 1 "src/scanner.rl"
/* scanner.rl -*-C-*- */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "frt_global.h"

#define RET goto ret;

#define STRIP(c) do { \
    strip_char = c;   \
    goto ret;         \
} while(0)


#line 28 "src/scanner.rl"



#line 25 "src/scanner.c"
static const char _StdTok_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1,
	3, 1, 4, 1, 5, 1, 17, 1,
	19, 1, 20, 1, 21, 1, 22, 1,
	23, 1, 24, 1, 25, 1, 26, 1,
	27, 1, 28, 1, 29, 1, 30, 1,
	31, 1, 32, 1, 33, 1, 34, 1,
	35, 1, 36, 1, 37, 2, 1, 18,
	2, 5, 6, 2, 5, 7, 2, 5,
	8, 2, 5, 9, 2, 5, 10, 2,
	5, 11, 2, 5, 12, 2, 5, 13,
	2, 5, 14, 2, 5, 15, 2, 5,
	16, 3, 5, 1, 12
};

static const short _StdTok_key_offsets[] = {
	0, 0, 0, 14, 28, 43, 57, 69,
	75, 85, 86, 92, 107, 128, 149, 170,
	197, 218, 220, 242, 264, 286, 308, 330,
	352, 374, 396, 418, 439, 466, 467, 474,
	501, 502, 515, 515, 537, 551, 565, 575,
	591, 607, 623, 646, 668, 671, 694, 717,
	740, 763, 787, 810, 833, 856, 878, 900,
	921, 929, 952, 977, 996, 1017, 1038, 1057,
	1078, 1100, 1122, 1144, 1166, 1188, 1210, 1232,
	1254, 1279, 1295, 1314, 1334, 1359, 1386, 1412,
	1438, 1463, 1476, 1502, 1528, 1554, 1580
};

static const unsigned char _StdTok_trans_keys[] = {
	33u, 46u, 61u, 64u, 35u, 39u, 42u, 43u,
	45u, 57u, 63u, 90u, 94u, 126u, 33u, 45u,
	61u, 63u, 35u, 39u, 42u, 43u, 47u, 57u,
	65u, 90u, 94u, 126u, 33u, 45u, 61u, 63u,
	91u, 35u, 39u, 42u, 43u, 47u, 57u, 65u,
	90u, 94u, 126u, 33u, 45u, 61u, 63u, 35u,
	39u, 42u, 43u, 47u, 57u, 65u, 90u, 94u,
	126u, 92u, 93u, 1u, 8u, 11u, 12u, 14u,
	31u, 33u, 90u, 94u, 127u, 1u, 9u, 11u,
	12u, 14u, 127u, 34u, 92u, 1u, 8u, 11u,
	12u, 14u, 31u, 33u, 127u, 64u, 1u, 9u,
	11u, 12u, 14u, 127u, 33u, 45u, 47u, 61u,
	63u, 35u, 39u, 42u, 43u, 48u, 57u, 65u,
	90u, 94u, 126u, 33u, 45u, 46u, 47u, 61u,
	63u, 64u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 35u,
	39u, 42u, 43u, 48u, 57u, 65u, 90u, 94u,
	96u, 97u, 122u, 123u, 126u, 33u, 45u, 46u,
	47u, 61u, 63u, 64u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 47u, 61u, 63u, 98u,
	99u, 101u, 103u, 105u, 109u, 110u, 111u, 35u,
	39u, 42u, 43u, 48u, 57u, 65u, 90u, 94u,
	96u, 97u, 122u, 123u, 126u, 33u, 45u, 46u,
	47u, 61u, 63u, 64u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 48u, 57u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 105u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 46u, 47u, 61u, 63u,
	64u, 111u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 100u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 61u, 63u, 64u, 111u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 110u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 46u, 47u, 61u, 63u,
	64u, 111u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 105u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 61u, 63u, 64u, 101u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 114u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 46u, 61u, 63u, 64u, 35u,
	39u, 42u, 43u, 45u, 47u, 48u, 57u, 65u,
	90u, 94u, 96u, 97u, 122u, 123u, 126u, 33u,
	45u, 47u, 61u, 63u, 98u, 99u, 101u, 103u,
	105u, 109u, 110u, 111u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 47u, 95u, 44u, 58u, 64u, 90u,
	97u, 122u, 33u, 45u, 47u, 61u, 63u, 98u,
	99u, 101u, 103u, 105u, 109u, 110u, 111u, 35u,
	39u, 42u, 43u, 48u, 57u, 65u, 90u, 94u,
	96u, 97u, 122u, 123u, 126u, 47u, 45u, 47u,
	58u, 64u, 95u, 44u, 46u, 48u, 57u, 65u,
	90u, 97u, 122u, 0u, 34u, 42u, 43u, 45u,
	47u, 61u, 63u, 102u, 104u, 33u, 39u, 48u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 33u, 46u, 61u, 64u, 35u, 39u, 42u,
	43u, 45u, 57u, 63u, 90u, 94u, 126u, 33u,
	46u, 61u, 63u, 35u, 39u, 42u, 43u, 45u,
	57u, 65u, 90u, 94u, 126u, 34u, 92u, 1u,
	8u, 11u, 12u, 14u, 31u, 33u, 127u, 33u,
	46u, 61u, 64u, 35u, 39u, 42u, 43u, 45u,
	47u, 48u, 57u, 63u, 90u, 94u, 126u, 33u,
	46u, 61u, 64u, 35u, 39u, 42u, 43u, 45u,
	47u, 48u, 57u, 63u, 90u, 94u, 126u, 33u,
	46u, 61u, 64u, 35u, 39u, 42u, 43u, 45u,
	47u, 48u, 57u, 63u, 90u, 94u, 126u, 33u,
	45u, 46u, 47u, 58u, 61u, 63u, 64u, 95u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 58u, 61u, 63u, 64u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 47u, 48u, 57u, 33u,
	45u, 46u, 47u, 58u, 61u, 63u, 64u, 122u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 121u, 123u, 126u, 33u, 45u,
	46u, 47u, 58u, 61u, 63u, 64u, 109u, 35u,
	39u, 42u, 43u, 48u, 57u, 65u, 90u, 94u,
	96u, 97u, 122u, 123u, 126u, 33u, 45u, 46u,
	47u, 58u, 61u, 63u, 64u, 117u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	58u, 61u, 63u, 64u, 118u, 35u, 39u, 42u,
	43u, 48u, 57u, 65u, 90u, 94u, 96u, 97u,
	122u, 123u, 126u, 33u, 45u, 46u, 47u, 58u,
	61u, 63u, 64u, 102u, 116u, 35u, 39u, 42u,
	43u, 48u, 57u, 65u, 90u, 94u, 96u, 97u,
	122u, 123u, 126u, 33u, 45u, 46u, 47u, 58u,
	61u, 63u, 64u, 108u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 46u, 47u, 58u, 61u,
	63u, 64u, 116u, 35u, 39u, 42u, 43u, 48u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 33u, 45u, 46u, 47u, 58u, 61u, 63u,
	64u, 103u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 95u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 61u, 63u, 64u, 95u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 35u, 39u, 42u, 43u, 48u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 47u, 95u, 44u, 58u, 64u, 90u, 97u,
	122u, 33u, 45u, 46u, 47u, 58u, 61u, 63u,
	64u, 95u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 38u, 39u, 45u, 46u, 47u, 58u, 61u,
	63u, 64u, 95u, 35u, 37u, 42u, 43u, 48u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 33u, 46u, 61u, 63u, 64u, 35u, 39u,
	42u, 43u, 45u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 46u, 61u, 63u,
	64u, 35u, 39u, 42u, 43u, 45u, 47u, 48u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 33u, 46u, 61u, 63u, 64u, 83u, 115u,
	35u, 39u, 42u, 43u, 45u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 46u,
	61u, 63u, 64u, 35u, 39u, 42u, 43u, 45u,
	57u, 65u, 90u, 94u, 96u, 97u, 122u, 123u,
	126u, 33u, 45u, 46u, 47u, 61u, 63u, 64u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 61u, 63u, 64u, 105u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 111u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 46u, 47u, 61u, 63u,
	64u, 100u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 111u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 45u,
	46u, 47u, 61u, 63u, 64u, 110u, 35u, 39u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 45u, 46u, 47u,
	61u, 63u, 64u, 105u, 35u, 39u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 45u, 46u, 47u, 61u, 63u,
	64u, 101u, 35u, 39u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 45u, 46u, 47u, 61u, 63u, 64u, 114u,
	35u, 39u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 38u,
	39u, 45u, 46u, 47u, 58u, 61u, 63u, 64u,
	95u, 35u, 37u, 42u, 43u, 48u, 57u, 65u,
	90u, 94u, 96u, 97u, 122u, 123u, 126u, 33u,
	46u, 61u, 64u, 83u, 115u, 35u, 39u, 42u,
	43u, 45u, 57u, 63u, 90u, 94u, 126u, 33u,
	45u, 61u, 63u, 91u, 35u, 39u, 42u, 43u,
	47u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 46u, 61u, 63u, 35u, 39u,
	42u, 43u, 45u, 47u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 38u,
	39u, 45u, 46u, 47u, 58u, 61u, 63u, 64u,
	95u, 35u, 37u, 42u, 43u, 48u, 57u, 65u,
	90u, 94u, 96u, 97u, 122u, 123u, 126u, 33u,
	38u, 39u, 45u, 46u, 47u, 58u, 61u, 63u,
	64u, 95u, 105u, 116u, 35u, 37u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 38u, 39u, 45u, 46u, 47u,
	58u, 61u, 63u, 64u, 95u, 108u, 35u, 37u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 38u, 39u, 45u,
	46u, 47u, 58u, 61u, 63u, 64u, 95u, 101u,
	35u, 37u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 38u,
	39u, 45u, 46u, 47u, 58u, 61u, 63u, 64u,
	95u, 35u, 37u, 42u, 43u, 48u, 57u, 65u,
	90u, 94u, 96u, 97u, 122u, 123u, 126u, 45u,
	47u, 58u, 64u, 95u, 44u, 46u, 48u, 57u,
	65u, 90u, 97u, 122u, 33u, 38u, 39u, 45u,
	46u, 47u, 58u, 61u, 63u, 64u, 95u, 112u,
	35u, 37u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 33u, 38u,
	39u, 45u, 46u, 47u, 58u, 61u, 63u, 64u,
	95u, 116u, 35u, 37u, 42u, 43u, 48u, 57u,
	65u, 90u, 94u, 96u, 97u, 122u, 123u, 126u,
	33u, 38u, 39u, 45u, 46u, 47u, 58u, 61u,
	63u, 64u, 95u, 116u, 35u, 37u, 42u, 43u,
	48u, 57u, 65u, 90u, 94u, 96u, 97u, 122u,
	123u, 126u, 33u, 38u, 39u, 45u, 46u, 47u,
	58u, 61u, 63u, 64u, 95u, 112u, 35u, 37u,
	42u, 43u, 48u, 57u, 65u, 90u, 94u, 96u,
	97u, 122u, 123u, 126u, 33u, 38u, 39u, 45u,
	46u, 47u, 58u, 61u, 63u, 64u, 95u, 115u,
	35u, 37u, 42u, 43u, 48u, 57u, 65u, 90u,
	94u, 96u, 97u, 122u, 123u, 126u, 0
};

static const char _StdTok_single_lengths[] = {
	0, 0, 4, 4, 5, 4, 2, 0,
	2, 1, 0, 5, 7, 7, 7, 13,
	7, 0, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 5, 13, 1, 1, 13,
	1, 5, 0, 10, 4, 4, 2, 4,
	4, 4, 9, 8, 1, 9, 9, 9,
	9, 10, 9, 9, 9, 8, 8, 7,
	2, 9, 11, 5, 5, 7, 5, 7,
	8, 8, 8, 8, 8, 8, 8, 8,
	11, 6, 5, 4, 11, 13, 12, 12,
	11, 5, 12, 12, 12, 12, 12
};

static const char _StdTok_range_lengths[] = {
	0, 0, 5, 5, 5, 5, 5, 3,
	4, 0, 3, 5, 7, 7, 7, 7,
	7, 1, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 8, 7, 0, 3, 7,
	0, 4, 0, 6, 5, 5, 4, 6,
	6, 6, 7, 7, 1, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	3, 7, 7, 7, 8, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 5, 7, 8, 7, 7, 7, 7,
	7, 4, 7, 7, 7, 7, 7
};

static const short _StdTok_index_offsets[] = {
	0, 0, 1, 11, 21, 32, 42, 50,
	54, 61, 63, 67, 78, 93, 108, 123,
	144, 159, 161, 177, 193, 209, 225, 241,
	257, 273, 289, 305, 319, 340, 342, 347,
	368, 370, 380, 381, 398, 408, 418, 425,
	436, 447, 458, 475, 491, 494, 511, 528,
	545, 562, 580, 597, 614, 631, 647, 663,
	678, 684, 701, 720, 733, 747, 762, 775,
	790, 806, 822, 838, 854, 870, 886, 902,
	918, 937, 949, 962, 975, 994, 1015, 1035,
	1055, 1074, 1084, 1104, 1124, 1144, 1164
};

static const char _StdTok_indicies[] = {
	0, 2, 3, 2, 4, 2, 2, 2,
	2, 2, 1, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 1, 5, 5, 5,
	5, 6, 5, 5, 5, 5, 5, 1,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 7, 8, 9, 6, 6, 6, 6,
	6, 1, 6, 6, 6, 1, 12, 13,
	11, 11, 11, 11, 10, 4, 10, 11,
	11, 11, 10, 2, 2, 2, 2, 2,
	2, 2, 15, 2, 2, 14, 2, 16,
	3, 2, 2, 2, 4, 2, 2, 17,
	17, 2, 17, 2, 1, 2, 16, 3,
	2, 2, 2, 4, 2, 2, 18, 18,
	2, 18, 2, 1, 2, 16, 19, 2,
	2, 2, 4, 2, 2, 18, 18, 2,
	18, 2, 1, 2, 2, 2, 2, 2,
	21, 22, 23, 24, 25, 26, 27, 28,
	2, 2, 18, 20, 2, 20, 2, 1,
	2, 16, 19, 2, 2, 2, 4, 2,
	2, 18, 29, 2, 29, 2, 1, 31,
	30, 2, 16, 19, 2, 2, 2, 4,
	32, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	33, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	34, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	35, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	36, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	29, 2, 2, 18, 18, 2, 18, 2,
	30, 2, 16, 19, 2, 2, 2, 4,
	37, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	38, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 16, 19, 2, 2, 2, 4,
	39, 2, 2, 18, 29, 2, 29, 2,
	1, 2, 3, 2, 2, 4, 2, 2,
	2, 40, 40, 2, 40, 2, 1, 2,
	2, 2, 2, 2, 21, 22, 23, 24,
	25, 26, 27, 28, 2, 2, 42, 20,
	2, 20, 2, 41, 43, 1, 44, 44,
	44, 44, 1, 2, 2, 2, 2, 2,
	46, 47, 48, 49, 50, 51, 52, 53,
	2, 2, 18, 45, 2, 45, 2, 1,
	55, 54, 56, 57, 44, 44, 56, 44,
	56, 56, 56, 54, 58, 59, 62, 61,
	63, 63, 61, 61, 61, 66, 67, 61,
	64, 65, 61, 65, 61, 60, 2, 3,
	2, 4, 2, 2, 2, 2, 2, 1,
	5, 69, 5, 5, 5, 5, 5, 5,
	5, 68, 12, 13, 11, 11, 11, 11,
	70, 2, 3, 2, 4, 2, 2, 2,
	71, 2, 2, 70, 2, 73, 2, 4,
	2, 2, 2, 71, 2, 2, 72, 2,
	3, 2, 4, 2, 2, 2, 15, 2,
	2, 74, 2, 76, 77, 2, 78, 2,
	2, 4, 80, 2, 2, 64, 79, 2,
	79, 2, 75, 2, 16, 19, 82, 83,
	2, 2, 4, 2, 2, 18, 18, 2,
	18, 2, 81, 84, 31, 81, 2, 16,
	19, 82, 83, 2, 2, 4, 29, 2,
	2, 18, 18, 2, 18, 2, 81, 2,
	16, 19, 82, 83, 2, 2, 4, 29,
	2, 2, 18, 18, 2, 18, 2, 81,
	2, 16, 19, 82, 83, 2, 2, 4,
	29, 2, 2, 18, 18, 2, 18, 2,
	81, 2, 16, 19, 82, 83, 2, 2,
	4, 29, 2, 2, 18, 18, 2, 18,
	2, 81, 2, 16, 19, 82, 83, 2,
	2, 4, 85, 29, 2, 2, 18, 18,
	2, 18, 2, 81, 2, 16, 19, 82,
	83, 2, 2, 4, 29, 2, 2, 18,
	18, 2, 18, 2, 81, 2, 16, 19,
	82, 83, 2, 2, 4, 29, 2, 2,
	18, 18, 2, 18, 2, 81, 2, 16,
	19, 82, 83, 2, 2, 4, 29, 2,
	2, 18, 18, 2, 18, 2, 81, 2,
	76, 19, 2, 2, 2, 4, 80, 2,
	2, 17, 17, 2, 17, 2, 75, 2,
	80, 3, 2, 2, 2, 4, 80, 2,
	2, 40, 40, 2, 40, 2, 75, 2,
	16, 19, 2, 2, 2, 4, 2, 2,
	42, 18, 2, 18, 2, 74, 86, 44,
	44, 44, 44, 81, 2, 76, 19, 2,
	78, 2, 2, 4, 80, 2, 2, 79,
	79, 2, 79, 2, 75, 2, 88, 89,
	76, 90, 2, 78, 2, 2, 92, 80,
	2, 2, 91, 93, 2, 93, 2, 87,
	2, 3, 2, 2, 4, 2, 2, 2,
	95, 2, 95, 2, 94, 2, 3, 2,
	2, 4, 2, 2, 2, 95, 95, 2,
	95, 2, 94, 2, 3, 2, 2, 4,
	98, 98, 2, 2, 2, 97, 2, 97,
	2, 96, 2, 3, 2, 2, 4, 2,
	2, 2, 97, 2, 97, 2, 1, 2,
	16, 90, 2, 2, 2, 4, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 32, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 33, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 34, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 35, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 36, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 37, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 38, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 16,
	90, 2, 2, 2, 4, 39, 2, 2,
	18, 29, 2, 29, 2, 99, 2, 88,
	100, 76, 19, 2, 78, 2, 2, 92,
	80, 2, 2, 91, 91, 2, 91, 2,
	87, 2, 3, 2, 4, 101, 101, 2,
	2, 2, 2, 2, 96, 5, 5, 5,
	5, 6, 5, 5, 5, 102, 5, 102,
	5, 94, 5, 69, 5, 5, 5, 5,
	5, 102, 102, 5, 102, 5, 68, 2,
	88, 89, 76, 19, 2, 78, 2, 2,
	92, 80, 2, 2, 91, 93, 2, 93,
	2, 87, 2, 88, 89, 76, 90, 2,
	78, 2, 2, 92, 80, 103, 104, 2,
	2, 91, 93, 2, 93, 2, 87, 2,
	88, 89, 76, 19, 2, 78, 2, 2,
	92, 80, 105, 2, 2, 91, 93, 2,
	93, 2, 87, 2, 88, 89, 76, 19,
	2, 78, 2, 2, 92, 80, 106, 2,
	2, 91, 93, 2, 93, 2, 87, 2,
	88, 89, 76, 19, 2, 107, 2, 2,
	92, 80, 2, 2, 91, 93, 2, 93,
	2, 87, 56, 108, 44, 44, 56, 44,
	56, 56, 56, 81, 2, 88, 89, 76,
	19, 2, 78, 2, 2, 92, 80, 106,
	2, 2, 91, 93, 2, 93, 2, 87,
	2, 88, 89, 76, 90, 2, 78, 2,
	2, 92, 80, 109, 2, 2, 91, 93,
	2, 93, 2, 87, 2, 88, 89, 76,
	19, 2, 78, 2, 2, 92, 80, 110,
	2, 2, 91, 93, 2, 93, 2, 87,
	2, 88, 89, 76, 19, 2, 78, 2,
	2, 92, 80, 111, 2, 2, 91, 93,
	2, 93, 2, 87, 2, 88, 89, 76,
	19, 2, 107, 2, 2, 92, 80, 106,
	2, 2, 91, 93, 2, 93, 2, 87,
	0
};

static const char _StdTok_trans_targs[] = {
	34, 35, 2, 3, 4, 37, 6, 35,
	7, 35, 35, 8, 9, 10, 35, 41,
	13, 53, 14, 15, 16, 18, 19, 20,
	21, 22, 24, 25, 26, 43, 35, 44,
	45, 46, 47, 48, 49, 50, 51, 52,
	54, 35, 55, 30, 56, 63, 64, 65,
	66, 67, 68, 69, 70, 71, 35, 33,
	56, 81, 0, 35, 35, 36, 38, 39,
	42, 58, 77, 83, 35, 5, 35, 40,
	35, 11, 35, 35, 12, 28, 29, 57,
	27, 35, 36, 17, 35, 23, 56, 35,
	59, 61, 31, 72, 74, 76, 35, 60,
	35, 62, 62, 35, 73, 36, 75, 78,
	82, 79, 80, 32, 81, 84, 85, 86
};

static const char _StdTok_trans_actions[] = {
	5, 51, 0, 0, 0, 11, 0, 39,
	0, 13, 49, 0, 0, 0, 47, 83,
	0, 68, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 74, 45, 0,
	74, 74, 74, 74, 74, 74, 74, 74,
	68, 43, 83, 0, 0, 77, 77, 77,
	77, 77, 77, 77, 77, 77, 41, 0,
	1, 0, 0, 15, 17, 86, 86, 86,
	68, 56, 56, 56, 19, 0, 37, 80,
	33, 0, 35, 25, 0, 0, 0, 68,
	0, 29, 89, 0, 53, 0, 3, 21,
	71, 59, 0, 56, 71, 56, 27, 71,
	23, 65, 62, 31, 59, 62, 11, 56,
	56, 56, 56, 0, 3, 56, 56, 56
};

static const char _StdTok_to_state_actions[] = {
	0, 7, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 7, 7, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0
};

static const char _StdTok_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 9, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0
};

static const short _StdTok_eof_trans[] = {
	0, 0, 2, 2, 2, 8, 2, 2,
	11, 11, 11, 15, 2, 2, 2, 2,
	2, 31, 2, 2, 2, 2, 2, 31,
	2, 2, 2, 2, 42, 2, 2, 2,
	55, 55, 0, 0, 2, 69, 71, 71,
	73, 75, 76, 82, 82, 82, 82, 82,
	82, 82, 82, 82, 82, 76, 76, 75,
	82, 76, 88, 95, 95, 97, 2, 100,
	100, 100, 100, 100, 100, 100, 100, 100,
	88, 97, 95, 69, 88, 88, 88, 88,
	88, 82, 88, 88, 88, 88, 88
};

static const int StdTok_start = 1;
static const int StdTok_error = 0;

static const int StdTok_en_frt_tokenizer = 35;
static const int StdTok_en_main = 1;


#line 31 "src/scanner.rl"

void frt_std_scan(const char *in,
                  char *out, size_t out_size,
                  const char **start,
                  const char **end,
                  int *token_size)
{
    int cs, act, top;
    int stack[32];
    char *ts = 0, *te = 0;


#line 549 "src/scanner.c"
	{
	cs = StdTok_start;
	top = 0;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 43 "src/scanner.rl"

    char *p = (char *)in, *pe = 0, *eof = pe;
    int skip = 0;
    int trunc = 0;
    char strip_char = 0;

    *end = 0;
    *start = 0;
    *token_size = 0;


#line 570 "src/scanner.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_acts = _StdTok_actions + _StdTok_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 4:
#line 1 "NONE"
	{ts = p;}
	break;
#line 591 "src/scanner.c"
		}
	}

	_keys = _StdTok_trans_keys + _StdTok_key_offsets[cs];
	_trans = _StdTok_index_offsets[cs];

	_klen = _StdTok_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _StdTok_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _StdTok_indicies[_trans];
_eof_trans:
	cs = _StdTok_trans_targs[_trans];

	if ( _StdTok_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _StdTok_actions + _StdTok_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 14 "src/url.rl"
	{ skip = p - ts; }
	break;
	case 1:
#line 26 "src/url.rl"
	{ trunc = 1; }
	break;
	case 2:
#line 27 "src/scanner.rl"
	{ p--; {stack[top++] = cs; cs = 35; goto _again;} }
	break;
	case 5:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 6:
#line 15 "src/scanner.in"
	{act = 2;}
	break;
	case 7:
#line 16 "src/scanner.in"
	{act = 3;}
	break;
	case 8:
#line 17 "src/scanner.in"
	{act = 4;}
	break;
	case 9:
#line 20 "src/scanner.in"
	{act = 5;}
	break;
	case 10:
#line 23 "src/scanner.in"
	{act = 6;}
	break;
	case 11:
#line 26 "src/scanner.in"
	{act = 7;}
	break;
	case 12:
#line 29 "src/scanner.in"
	{act = 8;}
	break;
	case 13:
#line 32 "src/scanner.in"
	{act = 9;}
	break;
	case 14:
#line 35 "src/scanner.in"
	{act = 10;}
	break;
	case 15:
#line 36 "src/scanner.in"
	{act = 11;}
	break;
	case 16:
#line 40 "src/scanner.in"
	{act = 13;}
	break;
	case 17:
#line 12 "src/scanner.in"
	{te = p+1;{ RET; }}
	break;
	case 18:
#line 29 "src/scanner.in"
	{te = p+1;{ RET; }}
	break;
	case 19:
#line 39 "src/scanner.in"
	{te = p+1;{ return; }}
	break;
	case 20:
#line 40 "src/scanner.in"
	{te = p+1;}
	break;
	case 21:
#line 12 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 22:
#line 15 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 23:
#line 16 "src/scanner.in"
	{te = p;p--;{ trunc = 1; RET; }}
	break;
	case 24:
#line 23 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 25:
#line 26 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 26:
#line 29 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 27:
#line 32 "src/scanner.in"
	{te = p;p--;{ STRIP('.'); }}
	break;
	case 28:
#line 35 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 29:
#line 36 "src/scanner.in"
	{te = p;p--;{ RET; }}
	break;
	case 30:
#line 40 "src/scanner.in"
	{te = p;p--;}
	break;
	case 31:
#line 12 "src/scanner.in"
	{{p = ((te))-1;}{ RET; }}
	break;
	case 32:
#line 15 "src/scanner.in"
	{{p = ((te))-1;}{ RET; }}
	break;
	case 33:
#line 23 "src/scanner.in"
	{{p = ((te))-1;}{ RET; }}
	break;
	case 34:
#line 29 "src/scanner.in"
	{{p = ((te))-1;}{ RET; }}
	break;
	case 35:
#line 35 "src/scanner.in"
	{{p = ((te))-1;}{ RET; }}
	break;
	case 36:
#line 40 "src/scanner.in"
	{{p = ((te))-1;}}
	break;
	case 37:
#line 1 "NONE"
	{	switch( act ) {
	case 2:
	{{p = ((te))-1;} RET; }
	break;
	case 3:
	{{p = ((te))-1;} trunc = 1; RET; }
	break;
	case 4:
	{{p = ((te))-1;} trunc = 2; RET; }
	break;
	case 5:
	{{p = ((te))-1;} RET; }
	break;
	case 6:
	{{p = ((te))-1;} RET; }
	break;
	case 7:
	{{p = ((te))-1;} RET; }
	break;
	case 8:
	{{p = ((te))-1;} RET; }
	break;
	case 9:
	{{p = ((te))-1;} STRIP('.'); }
	break;
	case 10:
	{{p = ((te))-1;} RET; }
	break;
	case 11:
	{{p = ((te))-1;} RET; }
	break;
	case 13:
	{{p = ((te))-1;}}
	break;
	}
	}
	break;
#line 836 "src/scanner.c"
		}
	}

_again:
	_acts = _StdTok_actions + _StdTok_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 3:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 849 "src/scanner.c"
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _StdTok_eof_trans[cs] > 0 ) {
		_trans = _StdTok_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

#line 54 "src/scanner.rl"

    if ( cs == StdTok_error )
                   fprintf(stderr, "PARSE ERROR\n" );
    else if ( ts ) fprintf(stderr, "STUFF LEFT: '%s'\n", ts);
    return;

 ret:
    {
        size_t __len = te - ts - skip - trunc;
        if (__len > out_size)
            __len = out_size;

        *start = ts;
        *end   = te;

        if (strip_char) {
            char *__p = ts + skip;
            char *__o = out;
            for (; __p < (ts + skip + __len); ++__p) {
                if (*__p != strip_char)
                    *__o++ = *__p;
            }
            *token_size = __o - out;
        }
        else {
            memcpy(out, ts + skip, __len);
            *token_size = __len;
        }

        out[*token_size] = 0;
    }
	(void)stack;
}
