#ifndef EMC_MODULE_H
#define EMC_MODULE_H

#include "ast.h"
#include <stdbool.h>
#include <stddef.h>

bool module_expr_to_string(const AstExpr *expr, char *buf, size_t buf_sz);
char *module_resolve_path(const char *module_name, const char *source_path);
bool module_resolve_all_imports(AstProgram *prog, const char *source_path, char *err_msg,
                                size_t err_sz);

#endif
