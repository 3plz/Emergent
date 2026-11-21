#ifndef EMC_CODEGEN_SCOPE_H
#define EMC_CODEGEN_SCOPE_H

#include "types.h"
#include <stddef.h>

typedef struct {
    char *name;
    TypeInfo type_info;
    char slot[64];
    bool is_moved;
    int moved_at_line;
    int moved_at_col;
    int depth;
} Symbol;

typedef struct {
    Symbol *symbols;
    size_t count;
    size_t capacity;
    int current_depth;
    int slot_counter;
} Scope;

void scope_init(Scope *s);
void scope_enter(Scope *s);
void scope_exit(Scope *s);
Symbol *scope_find(Scope *s, const char *name);
Symbol *scope_find_current(Scope *s, const char *name);
Symbol *scope_add(Scope *s, const char *name, TypeInfo ti, char *out_slot, size_t slot_sz);
void scope_free(Scope *s);

#endif
