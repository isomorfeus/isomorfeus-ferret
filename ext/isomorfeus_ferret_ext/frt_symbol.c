#include "frt_symbol.h"
#include "frt_hash.h"
#include "frt_internal.h"

static FrtHash *symbol_table = NULL;


void symbol_init()
{
    symbol_table = h_new_str(free, NULL);
    frt_register_for_cleanup(symbol_table, (free_ft)&h_destroy);
}
