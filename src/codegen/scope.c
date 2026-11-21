#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "scope.h"
#include "../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void scope_init(Scope *s) {
    assert(s);
    s->symbols = NULL;
    s->count = 0;
    s->capacity = 0;
    s->current_depth = 0;
    s->slot_counter = 1;
}

void scope_enter(Scope *s) {
    assert(s);
    s->current_depth++;
}

void scope_exit(Scope *s) {
    assert(s);
    assert(s->current_depth > 0);
    while (s->count > 0 && s->symbols[s->count - 1].depth == s->current_depth) {
        free(s->symbols[s->count - 1].name);
        s->count--;
    }
    s->current_depth--;
}

/* TODO: Add a name index if real-world scopes outgrow the reverse scan.  */
Symbol *scope_find(Scope *s, const char *name) {
    assert(s && name);
    for (size_t i = s->count; i > 0; i--) {
        if (strcmp(s->symbols[i - 1].name, name) == 0) {
            return &s->symbols[i - 1];
        }
    }
    return NULL;
}

Symbol *scope_find_current(Scope *s, const char *name) {
    assert(s && name);
    for (size_t i = s->count; i > 0; i--) {
        if (s->symbols[i - 1].depth < s->current_depth) {
            break;
        }
        if (strcmp(s->symbols[i - 1].name, name) == 0) {
            return &s->symbols[i - 1];
        }
    }
    return NULL;
}

Symbol *scope_add(Scope *s, const char *name, TypeInfo ti, char *out_slot, size_t slot_sz) {
    assert(s && name);
    da_grow(s->symbols, s->count, s->capacity);
    Symbol *sym = &s->symbols[s->count++];
    sym->name = xstrdup(name);
    sym->type_info = ti;
    sym->depth = s->current_depth;
    snprintf(sym->slot, sizeof(sym->slot), "%%v_%s_%d", name, s->slot_counter++);
    if (out_slot && slot_sz > 0) {
        snprintf(out_slot, slot_sz, "%s", sym->slot);
    }
    sym->is_moved = false;
    sym->moved_at_line = 0;
    sym->moved_at_col = 0;
    return sym;
}

void scope_free(Scope *s) {
    assert(s);
    for (size_t i = 0; i < s->count; i++) {
        free(s->symbols[i].name);
    }
    free(s->symbols);
    s->symbols = NULL;
    s->count = s->capacity = 0;
    s->current_depth = 0;
}
