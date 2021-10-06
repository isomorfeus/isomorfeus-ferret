#ifndef FRT_SYMBOL_H
#define FRT_SYMBOL_H

typedef char *FrtSymbol;

extern void symbol_init();

#define frt_sym_hash(sym) frt_str_hash(sym)

#endif
