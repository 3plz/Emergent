#ifndef EMC_DRIVER_DRIVER_H
#define EMC_DRIVER_DRIVER_H

#include "../codegen/codegen.h"
#include "../parser/ast.h"
#include <stdbool.h>
#include <stddef.h>

bool driver_run_process(const char *file, char *const argv[], int *out_exit_code, char *err_buf,
                        size_t err_sz);
bool driver_compile_and_link(const char *qbe_ir, const AstProgram *prog, const char *output_file,
                             CodegenResult *res);

#endif
