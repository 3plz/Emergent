#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parser/ast.h"
#include <stdbool.h>

typedef struct {
    bool had_error;
    char error_msg[512];
    int error_line;
    int error_column;
} CodegenResult;

char *codegen_generate_qbe(const AstProgram *prog, CodegenResult *res);
bool codegen_compile_and_link(const char *qbe_ir, const AstProgram *prog, const char *output_file,
                              CodegenResult *res);

#endif
