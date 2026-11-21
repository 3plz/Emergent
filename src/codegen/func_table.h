#ifndef EMC_CODEGEN_FUNC_TABLE_H
#define EMC_CODEGEN_FUNC_TABLE_H

#include "types.h"
#include <stddef.h>

typedef struct {
    char *name;
    char *module_name;
    char *mangled_name;
    TypeInfo ret_ti;
    /* Parameter declarations are borrowed from the AST.  */
    AstParam *const *params;
    size_t param_count;
    bool is_external;
} FuncSignature;

typedef enum {
    FUNC_FOUND,
    FUNC_UNKNOWN,
    FUNC_NO_MATCH,
    FUNC_AMBIGUOUS
} FuncResolution;

typedef struct {
    FuncSignature *funcs;
    size_t count;
    size_t capacity;
} FuncTable;

void func_table_init(FuncTable *ft);
bool func_table_add(FuncTable *ft, const char *module_name, const char *name, TypeInfo ret_ti,
                    AstParam *const *params, size_t param_count, bool is_external,
                    bool is_entry_point);
FuncResolution func_table_resolve(FuncTable *ft, const char *module_name,
                                  const char *current_module, const char *name,
                                  const TypeInfo *args, size_t arg_count, FuncSignature **out);
void func_table_free(FuncTable *ft);

#endif
