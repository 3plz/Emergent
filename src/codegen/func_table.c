/* Function signatures and overload resolution.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#include "func_table.h"
#include "../common.h"
#include "../utils/strbuf.h"
#include <stdlib.h>
#include <string.h>

void func_table_init(FuncTable *ft) {
    assert(ft);
    memset(ft, 0, sizeof(*ft));
}

static bool same_parameters(AstParam *const *a, AstParam *const *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!type_info_equal(get_type_info(a[i]->type), get_type_info(b[i]->type))) {
            return false;
        }
    }
    return true;
}

static FuncSignature *find_declaration(FuncTable *ft, const char *module_name, const char *name,
                                       AstParam *const *params, size_t param_count) {
    for (size_t i = 0; i < ft->count; i++) {
        FuncSignature *fn = &ft->funcs[i];
        if (strcmp(fn->module_name, module_name) == 0 && strcmp(fn->name, name) == 0 &&
            fn->param_count == param_count && same_parameters(fn->params, params, param_count)) {
            return fn;
        }
    }
    return NULL;
}

/* Length prefixes keep names distinct even when identifiers contain underscores.  */
static void append_type_name(StrBuf *name, const AstType *type) {
    AstTypeKind kind = type->kind;
    if (kind == TYPE_INT) {
        kind = TYPE_I32;
    } else if (kind == TYPE_UINT) {
        kind = TYPE_U32;
    }
    sb_printf(name, "_t%d", kind);
    if (kind == TYPE_POINTER) {
        append_type_name(name, type->base_type);
    } else if (kind == TYPE_CUSTOM) {
        sb_printf(name, "_%zu_%s", strlen(type->custom_name), type->custom_name);
    }
}

bool func_table_add(FuncTable *ft, const char *module_name, const char *name, TypeInfo ret_ti,
                    AstParam *const *params, size_t param_count, bool is_external,
                    bool is_entry_point) {
    assert(ft && module_name && name);
    if (find_declaration(ft, module_name, name, params, param_count)) {
        return false;
    }
    /* C symbols retain their linker names and cannot be overloaded.  */
    if (is_external) {
        for (size_t i = 0; i < ft->count; i++) {
            if (ft->funcs[i].is_external && strcmp(ft->funcs[i].name, name) == 0) {
                return false;
            }
        }
    }

    StrBuf mangled;
    sb_init(&mangled);
    if (is_external) {
        sb_printf(&mangled, "%s", name);
    } else if (is_entry_point) {
        sb_printf(&mangled, "main");
    } else {
        sb_printf(&mangled, "emg_%zu_%s_%zu_%s", strlen(module_name), module_name, strlen(name),
                  name);
        for (size_t i = 0; i < param_count; i++) {
            append_type_name(&mangled, params[i]->type);
        }
    }

    da_grow(ft->funcs, ft->count, ft->capacity);
    ft->funcs[ft->count++] = (FuncSignature){
        .name = xstrdup(name),
        .module_name = xstrdup(module_name),
        .mangled_name = mangled.data,
        .ret_ti = ret_ti,
        .params = params,
        .param_count = param_count,
        .is_external = is_external,
    };
    return true;
}

static bool matches_name(const FuncSignature *fn, const char *module_name, const char *name) {
    return strcmp(fn->name, name) == 0 &&
           (!module_name || strcmp(fn->module_name, module_name) == 0);
}

static bool accepts_arguments(const FuncSignature *fn, const TypeInfo *args, size_t count) {
    if (fn->param_count != count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (type_conversion_cost(args[i], get_type_info(fn->params[i]->type)) < 0) {
            return false;
        }
    }
    return true;
}

/* A candidate wins only if no argument converts worse and at least one converts better.  */
static bool better_match(const FuncSignature *a, const FuncSignature *b, const TypeInfo *args,
                         size_t count) {
    bool better = false;
    for (size_t i = 0; i < count; i++) {
        int ac = type_conversion_cost(args[i], get_type_info(a->params[i]->type));
        int bc = type_conversion_cost(args[i], get_type_info(b->params[i]->type));
        if (ac > bc) {
            return false;
        }
        better = better || ac < bc;
    }
    return better;
}

FuncResolution func_table_resolve(FuncTable *ft, const char *module_name,
                                  const char *current_module, const char *name,
                                  const TypeInfo *args, size_t arg_count, FuncSignature **out) {
    *out = NULL;
    /* Local declarations shadow imported functions for unqualified calls.  */
    if (!module_name && current_module) {
        for (size_t i = 0; i < ft->count; i++) {
            if (matches_name(&ft->funcs[i], current_module, name)) {
                module_name = current_module;
                break;
            }
        }
    }

    bool has_name = false;
    size_t winners = 0;
    for (size_t i = 0; i < ft->count; i++) {
        FuncSignature *fn = &ft->funcs[i];
        if (!matches_name(fn, module_name, name)) {
            continue;
        }
        has_name = true;
        if (!accepts_arguments(fn, args, arg_count)) {
            continue;
        }

        bool dominated = false;
        for (size_t j = 0; j < ft->count; j++) {
            FuncSignature *other = &ft->funcs[j];
            if (j != i && matches_name(other, module_name, name) &&
                accepts_arguments(other, args, arg_count) &&
                better_match(other, fn, args, arg_count)) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            *out = fn;
            winners++;
        }
    }

    if (winners == 1) {
        return FUNC_FOUND;
    }
    *out = NULL;
    if (winners > 1) {
        return FUNC_AMBIGUOUS;
    }
    return has_name ? FUNC_NO_MATCH : FUNC_UNKNOWN;
}

void func_table_free(FuncTable *ft) {
    assert(ft);
    for (size_t i = 0; i < ft->count; i++) {
        free(ft->funcs[i].name);
        free(ft->funcs[i].module_name);
        free(ft->funcs[i].mangled_name);
    }
    free(ft->funcs);
    memset(ft, 0, sizeof(*ft));
}
