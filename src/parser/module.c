/* Module resolution and AST merging for Emergent programs.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "module.h"
#include "../common.h"
#include "../lexer/lexer.h"
#include "../utils/file.h"
#include "ast.h"
#include "parser.h"
#include <libgen.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LOADED_MODULES 128

typedef struct {
    char *paths[MAX_LOADED_MODULES];
    size_t count;
} LoadedModules;

/* FIXME: Replace the fixed array and linear scan with a growable hash set.  */
static bool is_module_loaded(const LoadedModules *lm, const char *path) {
    char canon[512];
    if (!realpath(path, canon)) {
        snprintf(canon, sizeof(canon), "%s", path);
    }

    for (size_t i = 0; i < lm->count; i++) {
        if (strcmp(lm->paths[i], canon) == 0) {
            return true;
        }
    }
    return false;
}

static void mark_module_loaded(LoadedModules *lm, const char *path) {
    if (lm->count >= MAX_LOADED_MODULES) {
        return;
    }
    char canon[512];
    if (!realpath(path, canon)) {
        snprintf(canon, sizeof(canon), "%s", path);
    }

    lm->paths[lm->count++] = xstrdup(canon);
}

static void free_loaded_modules(LoadedModules *lm) {
    for (size_t i = 0; i < lm->count; i++) {
        free(lm->paths[i]);
    }
    lm->count = 0;
}

bool module_expr_to_string(const AstExpr *expr, char *buf, size_t buf_sz) {
    if (!expr || !buf || buf_sz == 0) {
        return false;
    }

    switch (expr->kind) {
    case EXPR_IDENTIFIER:
        snprintf(buf, buf_sz, "%s", expr->ident_name ? expr->ident_name : "");
        return true;

    case EXPR_STRING_LIT:
        snprintf(buf, buf_sz, "%s", expr->str_val ? expr->str_val : "");
        return true;

    case EXPR_PATH: {
        char target_buf[256];
        if (!module_expr_to_string(expr->path.target, target_buf, sizeof(target_buf))) {
            return false;
        }
        snprintf(buf, buf_sz, "%s:%s", target_buf, expr->path.subpath ? expr->path.subpath : "");
        return true;
    }

    case EXPR_MEMBER: {
        char target_buf[256];
        if (!module_expr_to_string(expr->member.target, target_buf, sizeof(target_buf))) {
            return false;
        }
        snprintf(buf, buf_sz, "%s.%s", target_buf, expr->member.member ? expr->member.member : "");
        return true;
    }

    default:
        return false;
    }
}

/* Return an owned path, or NULL when no candidate exists.  */
char *module_resolve_path(const char *module_name, const char *source_path) {
    if (!module_name || !*module_name) {
        return NULL;
    }

    char candidate[512];

    if (strncmp(module_name, "std:", 4) == 0) {
        const char *p = module_name + 4;
        char sub_dir[128];
        snprintf(sub_dir, sizeof(sub_dir), "%s", p);

        /* Map std:foo:bar to std/foo/bar on disk.  */
        for (size_t i = 0; i < strlen(sub_dir); i++) {
            if (sub_dir[i] == ':') {
                sub_dir[i] = '/';
            }
        }

        snprintf(candidate, sizeof(candidate), "std/%s/%s.em", sub_dir, p);
        if (access(candidate, R_OK) == 0) {
            return xstrdup(candidate);
        }

        /* Some modules use a directory entry file such as std/io/io.em.  */
        char *slash = strchr(sub_dir, '/');
        if (slash) {
            *slash = '\0';
            snprintf(candidate, sizeof(candidate), "std/%s/%s.em", sub_dir, sub_dir);
            if (access(candidate, R_OK) == 0) {
                return xstrdup(candidate);
            }

            snprintf(candidate, sizeof(candidate), "std/%s.em", sub_dir);
            if (access(candidate, R_OK) == 0) {
                return xstrdup(candidate);
            }
        }

        snprintf(candidate, sizeof(candidate), "std/%s.em", sub_dir);
        if (access(candidate, R_OK) == 0) {
            return xstrdup(candidate);
        }

        const char *env_std = getenv("EMC_STD_PATH");
        if (env_std) {
            snprintf(candidate, sizeof(candidate), "%s/%s/%s.em", env_std, sub_dir, sub_dir);
            if (access(candidate, R_OK) == 0) {
                return xstrdup(candidate);
            }

            snprintf(candidate, sizeof(candidate), "%s/%s.em", env_std, sub_dir);
            if (access(candidate, R_OK) == 0) {
                return xstrdup(candidate);
            }
        }
    } else {
        if (source_path) {
            char dir_copy[512];
            snprintf(dir_copy, sizeof(dir_copy), "%s", source_path);
            char *dir = dirname(dir_copy);
            if (dir) {
                snprintf(candidate, sizeof(candidate), "%s/%s.em", dir, module_name);
                if (access(candidate, R_OK) == 0) {
                    return xstrdup(candidate);
                }

                snprintf(candidate, sizeof(candidate), "%s/%s/%s.em", dir, module_name,
                         module_name);
                if (access(candidate, R_OK) == 0) {
                    return xstrdup(candidate);
                }
            }
        }

        snprintf(candidate, sizeof(candidate), "./%s.em", module_name);
        if (access(candidate, R_OK) == 0) {
            return xstrdup(candidate);
        }

        snprintf(candidate, sizeof(candidate), "./%s/%s.em", module_name, module_name);
        if (access(candidate, R_OK) == 0) {
            return xstrdup(candidate);
        }
    }

    return NULL;
}

static bool is_import_call(const AstExpr *expr) {
    if (!expr || expr->kind != EXPR_CALL) {
        return false;
    }

    const AstExpr *callee = expr->call.callee;
    if (!callee || callee->kind != EXPR_MEMBER || !callee->member.member ||
        strcmp(callee->member.member, "add") != 0) {
        return false;
    }

    const AstExpr *target = callee->member.target;
    return target && target->kind == EXPR_IDENTIFIER && target->ident_name &&
           (strcmp(target->ident_name, "mod") == 0 || strcmp(target->ident_name, "sub") == 0);
}

static bool expr_contains_import(const AstExpr *expr) {
    if (!expr) {
        return false;
    }
    if (is_import_call(expr)) {
        return true;
    }

    switch (expr->kind) {
    case EXPR_CALL:
        if (expr_contains_import(expr->call.callee)) {
            return true;
        }
        for (size_t i = 0; i < expr->call.arg_count; i++) {
            if (expr_contains_import(expr->call.args[i])) {
                return true;
            }
        }
        return false;
    case EXPR_MEMBER:
        return expr_contains_import(expr->member.target);
    case EXPR_PATH:
        return expr_contains_import(expr->path.target);
    case EXPR_BINARY:
        return expr_contains_import(expr->binary.left) || expr_contains_import(expr->binary.right);
    case EXPR_UNARY:
        return expr_contains_import(expr->unary.operand);
    default:
        return false;
    }
}

/* Inspect the entire conditional so imports cannot hide in nested expressions or branches.  */
static bool stmt_contains_import(const AstStmt *stmt) {
    if (!stmt) {
        return false;
    }

    switch (stmt->kind) {
    case STMT_VAR_DECL:
        return expr_contains_import(stmt->var_decl.init_expr);
    case STMT_ASSIGN:
        return expr_contains_import(stmt->assign.target) || expr_contains_import(stmt->assign.value);
    case STMT_MOVE:
        return expr_contains_import(stmt->move.src_expr);
    case STMT_RETURN:
        return expr_contains_import(stmt->return_stmt.expr);
    case STMT_EXPR:
        return expr_contains_import(stmt->expr_stmt);
    case STMT_IF:
        if (expr_contains_import(stmt->if_stmt.cond)) {
            return true;
        }
        for (size_t i = 0; i < stmt->if_stmt.then_count; i++) {
            if (stmt_contains_import(stmt->if_stmt.then_stmts[i])) {
                return true;
            }
        }
        for (size_t i = 0; i < stmt->if_stmt.else_count; i++) {
            if (stmt_contains_import(stmt->if_stmt.else_stmts[i])) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

static bool resolve_imports_recursive(AstProgram *program, const char *source_path,
                                      LoadedModules *lm, char *err_msg, size_t err_sz) {
    if (!program) {
        return true;
    }

    for (size_t b = 0; b < program->block_count; b++) {
        AstBlock *block = program->blocks[b];
        for (size_t s = 0; s < block->stmt_count; s++) {
            AstStmt *stmt = block->stmts[s];
            if (stmt->kind == STMT_IF) {
                if (stmt_contains_import(stmt)) {
                    snprintf(err_msg, err_sz, "Adding a module based on the condition is prohibited");
                    return false;
                }
                continue;
            }
            if (stmt->kind != STMT_EXPR || !is_import_call(stmt->expr_stmt)) {
                continue;
            }

            const char *target_name = stmt->expr_stmt->call.callee->member.target->ident_name;
            if (stmt->expr_stmt->call.arg_count != 1) {
                snprintf(err_msg, err_sz, "%s.add requires exactly one module argument",
                         target_name);
                return false;
            }

            char mod_name[256];
            if (!module_expr_to_string(stmt->expr_stmt->call.args[0], mod_name, sizeof(mod_name))) {
                snprintf(err_msg, err_sz, "Failed to express source module to string");
                return false;
            }

            char *mod_file = module_resolve_path(mod_name, source_path);
            if (!mod_file) {
                snprintf(err_msg, err_sz, "Failed to find module '%s'", mod_name);
                return false;
            }

            if (is_module_loaded(lm, mod_file)) {
                free(mod_file);
                continue;
            }

            mark_module_loaded(lm, mod_file);

            char *source = read_source_file(mod_file);
            if (!source) {
                snprintf(err_msg, err_sz, "Failed to read module '%s' at path '%s'", mod_name,
                         mod_file);
                free(mod_file);
                return false;
            }

            Lexer lexer;
            lexer_init(&lexer, source);
            Parser parser;
            parser_init(&parser, &lexer);
            AstProgram *sub_prog = parser_parse(&parser);

            if (parser.had_error) {
                snprintf(err_msg, err_sz, "Syntax error while parsing module '%s'", mod_file);
                ast_program_free(sub_prog);
                free(source);
                free(mod_file);
                return false;
            }

            if (!resolve_imports_recursive(sub_prog, mod_file, lm, err_msg, err_sz)) {
                ast_program_free(sub_prog);
                free(source);
                free(mod_file);
                return false;
            }

            ast_program_collect_link_libs(sub_prog);
            for (size_t l = 0; l < sub_prog->link_lib_count; l++) {
                ast_program_add_link_lib(program, sub_prog->link_libs[l]);
            }

            for (size_t d = 0; d < sub_prog->doc_block_count; d++) {
                ast_program_add_doc_block(program, sub_prog->doc_blocks[d]);
                sub_prog->doc_blocks[d] = NULL;
            }
            sub_prog->doc_block_count = 0;

            for (size_t e = 0; e < sub_prog->extern_block_count; e++) {
                ast_program_add_extern_block(program, sub_prog->extern_blocks[e]);
                sub_prog->extern_blocks[e] = NULL;
            }
            sub_prog->extern_block_count = 0;

            for (size_t cb = 0; cb < sub_prog->block_count; cb++) {
                AstBlock *b_to_move = sub_prog->blocks[cb];
                if (b_to_move && strcmp(b_to_move->name, "Init") != 0) {
                    ast_program_add_block(program, b_to_move);
                    sub_prog->blocks[cb] = NULL;
                }
            }

            ast_program_free(sub_prog);
            free(source);
            free(mod_file);
        }
    }

    return true;
}

bool module_resolve_all_imports(AstProgram *program, const char *source_path, char *err_msg,
                                size_t err_sz) {
    if (!program) {
        return false;
    }

    LoadedModules lm = {0};
    if (source_path) {
        mark_module_loaded(&lm, source_path);
    }

    bool ok = resolve_imports_recursive(program, source_path, &lm, err_msg, err_sz);
    free_loaded_modules(&lm);
    return ok;
}
