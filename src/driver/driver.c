/* External toolchain execution and compilation driver.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

bool driver_run_process(const char *file, char *const argv[], int *out_exit_code, char *err_buf,
                        size_t err_sz) {
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(err_buf, err_sz, "Failed to spawn process (fork)");
        return false;
    }
    if (pid == 0) {
        execvp(file, argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        snprintf(err_buf, err_sz, "Failed to wait for process completion (waitpid)");
        return false;
    }
    if (WIFEXITED(status)) {
        *out_exit_code = WEXITSTATUS(status);
        return (*out_exit_code == 0);
    }
    *out_exit_code = -1;
    return false;
}

bool driver_compile_and_link(const char *qbe_ir, const AstProgram *prog, const char *output_file,
                             CodegenResult *res) {
    char ssa_path[] = "/tmp/emc_ssa_XXXXXX";
    int ssa_fd = mkstemp(ssa_path);
    if (ssa_fd < 0) {
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "Failed to create temporary SSA file (mkstemp)");
        res->had_error = true;
        return false;
    }

    size_t ir_len = strlen(qbe_ir);
    if (write(ssa_fd, qbe_ir, ir_len) != (ssize_t)ir_len) {
        close(ssa_fd);
        unlink(ssa_path);
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "Failed to write QBE IR to temporary file");
        res->had_error = true;
        return false;
    }
    close(ssa_fd);

    char asm_path[] = "/tmp/emc_asm_XXXXXX";
    int asm_fd = mkstemp(asm_path);
    if (asm_fd < 0) {
        unlink(ssa_path);
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "Failed to create temporary assembly file (mkstemp)");
        res->had_error = true;
        return false;
    }
    close(asm_fd);

    char *qbe_argv[] = {"qbe", "-o", asm_path, ssa_path, NULL};
    int qbe_exit = 0;
    if (!driver_run_process("qbe", qbe_argv, &qbe_exit, res->error_msg, sizeof(res->error_msg))) {
        unlink(ssa_path);
        unlink(asm_path);
        snprintf(res->error_msg, sizeof(res->error_msg), "QBE compiler error (exit code %d)",
                 qbe_exit);
        res->had_error = true;
        return false;
    }
    unlink(ssa_path);

    char c_path[64] = "";
    bool has_c = false;
    if (prog && prog->extern_block_count > 0) {
        snprintf(c_path, sizeof(c_path), "/tmp/emc_c_XXXXXX");
        int c_fd = mkstemp(c_path);
        if (c_fd >= 0) {
            has_c = true;
            for (size_t i = 0; i < prog->extern_block_count; i++) {
                if (prog->extern_blocks[i]->raw_code) {
                    write(c_fd, prog->extern_blocks[i]->raw_code,
                          strlen(prog->extern_blocks[i]->raw_code));
                    write(c_fd, "\n", 1);
                }
            }
            close(c_fd);
        }
    }

    char *cc_argv[64];
    int arg_idx = 0;
    cc_argv[arg_idx++] = "cc";
    cc_argv[arg_idx++] = "-x";
    cc_argv[arg_idx++] = "assembler";
    cc_argv[arg_idx++] = asm_path;
    if (has_c) {
        cc_argv[arg_idx++] = "-x";
        cc_argv[arg_idx++] = "c";
        cc_argv[arg_idx++] = c_path;
    }
    cc_argv[arg_idx++] = "-no-pie";

    char lib_args[32][64];
    if (prog) {
        for (size_t i = 0; i < prog->link_lib_count && i < 32 && arg_idx < 60; i++) {
            snprintf(lib_args[i], sizeof(lib_args[i]), "-l%s", prog->link_libs[i]);
            cc_argv[arg_idx++] = lib_args[i];
        }
    }

    cc_argv[arg_idx++] = "-o";
    cc_argv[arg_idx++] = (char *)output_file;
    cc_argv[arg_idx] = NULL;

    int cc_exit = 0;
    if (!driver_run_process("cc", cc_argv, &cc_exit, res->error_msg, sizeof(res->error_msg))) {
        unlink(asm_path);
        if (has_c) {
            unlink(c_path);
        }
        snprintf(res->error_msg, sizeof(res->error_msg), "CC linker error (exit code %d)", cc_exit);
        res->had_error = true;
        return false;
    }
    unlink(asm_path);
    if (has_c) {
        unlink(c_path);
    }

    return true;
}

bool codegen_compile_and_link(const char *qbe_ir, const AstProgram *prog, const char *output_file,
                              CodegenResult *res) {
    return driver_compile_and_link(qbe_ir, prog, output_file, res);
}
