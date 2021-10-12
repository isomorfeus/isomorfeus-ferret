#include "frt_symbol.h"
#include "frt_hash.h"

static FrtHash *symbol_table = NULL;


void symbol_init()
{
    symbol_table = frt_h_new_str(free, NULL);
    frt_register_for_cleanup(symbol_table, (frt_free_ft)&frt_h_destroy);
}
