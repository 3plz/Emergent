/* Driver and command-line interface entry point for the Emergent compiler.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "codegen/codegen.h"
#include "doc/doc_man.h"
#include "driver/driver.h"
#include "lexer/lexer.h"
#include "parser/module.h"
#include "parser/parser.h"
#include "utils/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void print_usage(const char *prog_name) {
    printf("Emergent Compiler (emc)\n\n");
    printf("Usage:\n");
    printf("  %s <file.em> [options]         Compile Emergent source file\n", prog_name);
    printf("  %s man [file.em] <submodule>   View submodule documentation\n", prog_name);
    printf("  %s run <file.em>               Compile and immediately run\n\n", prog_name);
    printf("Compilation options:\n");
    printf("  -o <path>       Output binary path (default: ./prog)\n");
    printf("  -r, --run       Run compiled executable after build\n");
    printf("  --dump-ast      Print AST tree\n");
    printf("  --dump-ir       Print QBE Intermediate Representation\n");
    printf("  -h, --help      Display this help message\n\n");
    printf("Man command options:\n");
    printf("  --json          Output documentation in JSON format\n");
    printf("  --all           Show documentation for all submodules in file\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "man") == 0) {
        return handle_man_command(argc - 1, argv + 1);
    }

    const char *path = NULL;
    const char *out_path = "./prog";
    bool run_after = false;
    bool dump_ast = false;
    bool dump_ir = false;

    int start_arg = 1;
    if (strcmp(argv[1], "run") == 0) {
        run_after = true;
        start_arg = 2;
    }

    for (int i = start_arg; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) {
            run_after = true;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "--dump-ir") == 0 || strcmp(argv[i], "--emit-qbe") == 0) {
            dump_ir = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    if (!path) {
        path = "references/hello.em";
    }

    char *source = read_source_file(path);
    if (source == NULL) {
        return 1;
    }

    Lexer lexer;
    lexer_init(&lexer, source);

    printf("--- Compiling file: %s ---\n", path);

    Parser parser;
    parser_init(&parser, &lexer);

    AstProgram *program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "\nCompilation aborted: syntax errors detected.\n");
        ast_program_free(program);
        free(source);
        return 1;
    }

    char mod_err[512] = "";
    if (!module_resolve_all_imports(program, path, mod_err, sizeof(mod_err))) {
        fprintf(stderr, "\nModule resolution error: %s\n", mod_err);
        ast_program_free(program);
        free(source);
        return 1;
    }

    ast_program_collect_link_libs(program);

    if (dump_ast) {
        printf("--- Abstract Syntax Tree (AST) ---\n");
        ast_print(program);
        printf("\n");
    }

    CodegenResult gen_res;
    char *qbe_ir = codegen_generate_qbe(program, &gen_res);

    if (gen_res.had_error) {
        fprintf(stderr, "[Line %d, column %d] Semantic / codegen error: %s\n", gen_res.error_line,
                gen_res.error_column, gen_res.error_msg);
        ast_program_free(program);
        free(source);
        return 1;
    }

    if (dump_ir) {
        printf("--- Generated QBE IR ---\n");
        printf("%s\n", qbe_ir);
    }

    if (!driver_compile_and_link(qbe_ir, program, out_path, &gen_res)) {
        fprintf(stderr, "Build error: %s\n", gen_res.error_msg);
        free(qbe_ir);
        ast_program_free(program);
        free(source);
        return 1;
    }

    int final_exit_code = 0;

    if (run_after) {
        fflush(stdout);
        char exec_path[1024];
        if (strchr(out_path, '/') == NULL) {
            snprintf(exec_path, sizeof(exec_path), "./%s", out_path);
        } else {
            snprintf(exec_path, sizeof(exec_path), "%s", out_path);
        }

        char *run_args[] = {exec_path, NULL};
        int run_exit = 0;
        char run_err[512] = "";

        if (!driver_run_process(exec_path, run_args, &run_exit, run_err, sizeof(run_err))) {
            if (run_exit > 0) {
                printf("[Exit code: %d]\n", run_exit);
                final_exit_code = run_exit;
            } else {
                fprintf(stderr, "Execution error: %s\n", run_err);
                final_exit_code = 1;
            }
        }
    } else {
        printf("Successfully compiled to %s\n", out_path);
    }

    free(qbe_ir);
    ast_program_free(program);
    free(source);
    return final_exit_code;
}
