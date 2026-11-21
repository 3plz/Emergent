/* Documentation manual page formatter and CLI browser for Emergent.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "doc_man.h"
#include "../common.h"
#include "../lexer/lexer.h"
#include "../parser/module.h"
#include "../parser/parser.h"
#include "../utils/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_json_escaped(const char *s) {
    if (!s) {
        return;
    }
    for (const char *p = s; *p; p++) {
        if (*p == '"') {
            printf("\\\"");
        } else if (*p == '\\') {
            printf("\\\\");
        } else if (*p == '\n') {
            printf("\\n");
        } else if (*p == '\r') {
            printf("\\r");
        } else if (*p == '\t') {
            printf("\\t");
        } else {
            putchar(*p);
        }
    }
}

static void print_man_indent(const char *text, int indent_spaces) {
    if (!text) {
        return;
    }
    const char *line_start = text;
    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        size_t len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);

        for (int i = 0; i < indent_spaces; i++) {
            putchar(' ');
        }
        printf("%.*s\n", (int)len, line_start);

        if (!line_end) {
            break;
        }
        line_start = line_end + 1;
    }
}

static AstBlock *find_block(const AstProgram *prog, const char *name) {
    AstBlock *found = NULL;
    for (size_t i = 0; i < prog->block_count; i++) {
        if (strcmp(prog->blocks[i]->name, name) == 0) {
            /* Use the documented signature for an overloaded function.  */
            if (found) {
                return NULL;
            }
            found = prog->blocks[i];
        }
    }
    return found;
}

static void print_doc_json(const AstProgram *prog, const AstDocBlock *doc) {
    printf("  {\n");
    printf("    \"module\": \"");
    print_json_escaped(prog->module_name);
    printf("\",\n");

    printf("    \"submodule\": \"");
    print_json_escaped(doc->target_name);
    printf("\",\n");

    AstBlock *code_block = find_block(prog, doc->target_name);
    printf("    \"signature\": \"-[%s]", doc->target_name);
    if (code_block && code_block->param_count > 0) {
        printf(" {");
        for (size_t p = 0; p < code_block->param_count; p++) {
            if (p > 0) {
                printf(", ");
            }
            printf("%s: %s", code_block->params[p]->name,
                   ast_type_to_string(code_block->params[p]->type));
        }
        printf("}");
    } else if (doc->arg_count > 0) {
        printf(" {");
        for (size_t a = 0; a < doc->arg_count; a++) {
            if (a > 0) {
                printf(", ");
            }
            printf("%s: %s", doc->args[a]->name, doc->args[a]->type);
        }
        printf("}");
    }
    if (code_block && code_block->ret_type) {
        printf(" -> %s", ast_type_to_string(code_block->ret_type));
    }
    printf("\",\n");

    printf("    \"description\": \"");
    print_json_escaped(doc->description ? doc->description : "");
    printf("\",\n");

    printf("    \"arguments\": [\n");
    for (size_t a = 0; a < doc->arg_count; a++) {
        printf("      {\n");
        printf("        \"name\": \"");
        print_json_escaped(doc->args[a]->name);
        printf("\",\n");
        printf("        \"type\": \"");
        print_json_escaped(doc->args[a]->type);
        printf("\",\n");
        printf("        \"description\": \"");
        print_json_escaped(doc->args[a]->desc);
        printf("\"\n");
        printf("      }%s\n", (a == doc->arg_count - 1) ? "" : ",");
    }
    printf("    ],\n");

    printf("    \"returns\": [\n");
    for (size_t r = 0; r < doc->return_count; r++) {
        printf("      {\n");
        printf("        \"name\": \"");
        print_json_escaped(doc->returns[r]->name);
        printf("\",\n");
        printf("        \"type\": \"");
        print_json_escaped(doc->returns[r]->type);
        printf("\",\n");
        printf("        \"description\": \"");
        print_json_escaped(doc->returns[r]->desc);
        printf("\"\n");
        printf("      }%s\n", (r == doc->return_count - 1) ? "" : ",");
    }
    printf("    ]\n");
    printf("  }");
}

static void print_doc_man_page(const AstProgram *prog, const AstDocBlock *doc) {
    bool is_tty = isatty(STDOUT_FILENO);
    const char *bold_on = is_tty ? "\033[1m" : "";
    const char *bold_off = is_tty ? "\033[0m" : "";
    const char *cyan_on = is_tty ? "\033[1;36m" : "";
    const char *green_on = is_tty ? "\033[1;32m" : "";

    AstBlock *code_block = find_block(prog, doc->target_name);

    printf("%sNAME%s\n", bold_on, bold_off);
    printf("    %s%s%s - %s submodule\n\n", cyan_on, doc->target_name, bold_off, prog->module_name);

    printf("%sSYNOPSIS%s\n", bold_on, bold_off);
    printf("    %s-[%s]%s", green_on, doc->target_name, bold_off);
    if (code_block && code_block->param_count > 0) {
        printf(" {");
        for (size_t p = 0; p < code_block->param_count; p++) {
            if (p > 0) {
                printf(", ");
            }
            printf("%s: %s", code_block->params[p]->name,
                   ast_type_to_string(code_block->params[p]->type));
        }
        printf("}");
    } else if (doc->arg_count > 0) {
        printf(" {");
        for (size_t a = 0; a < doc->arg_count; a++) {
            if (a > 0) {
                printf(", ");
            }
            if (strlen(doc->args[a]->name) > 0) {
                printf("%s: %s", doc->args[a]->name, doc->args[a]->type);
            } else {
                printf("%s", doc->args[a]->type);
            }
        }
        printf("}");
    }
    if (code_block && code_block->ret_type) {
        printf(" -> %s", ast_type_to_string(code_block->ret_type));
    }
    printf("\n\n");

    if (doc->description && strlen(doc->description) > 0) {
        printf("%sDESCRIPTION%s\n", bold_on, bold_off);
        print_man_indent(doc->description, 4);
        printf("\n");
    }

    if (doc->arg_count > 0) {
        printf("%sARGUMENTS%s\n", bold_on, bold_off);
        for (size_t a = 0; a < doc->arg_count; a++) {
            if (strlen(doc->args[a]->name) > 0 && strlen(doc->args[a]->type) > 0) {
                printf("    %s%s%s : %s\n", bold_on, doc->args[a]->name, bold_off,
                       doc->args[a]->type);
            } else if (strlen(doc->args[a]->name) > 0) {
                printf("    %s%s%s\n", bold_on, doc->args[a]->name, bold_off);
            } else if (strlen(doc->args[a]->type) > 0) {
                printf("    %s%s%s\n", bold_on, doc->args[a]->type, bold_off);
            }

            if (strlen(doc->args[a]->desc) > 0) {
                print_man_indent(doc->args[a]->desc, 8);
            }
        }
        printf("\n");
    }

    if (doc->return_count > 0) {
        printf("%sRETURNS%s\n", bold_on, bold_off);
        for (size_t r = 0; r < doc->return_count; r++) {
            if (strlen(doc->returns[r]->name) > 0 && strlen(doc->returns[r]->type) > 0) {
                printf("    %s%s%s : %s\n", bold_on, doc->returns[r]->name, bold_off,
                       doc->returns[r]->type);
            } else if (strlen(doc->returns[r]->type) > 0) {
                printf("    %s%s%s\n", bold_on, doc->returns[r]->type, bold_off);
            } else if (strlen(doc->returns[r]->name) > 0) {
                printf("    %s%s%s\n", bold_on, doc->returns[r]->name, bold_off);
            }

            if (strlen(doc->returns[r]->desc) > 0) {
                print_man_indent(doc->returns[r]->desc, 8);
            }
        }
        printf("\n");
    }

    for (size_t f = 0; f < doc->extra_field_count; f++) {
        char upper_key[64];
        size_t klen = strlen(doc->extra_fields[f]->key);
        if (klen >= sizeof(upper_key)) {
            klen = sizeof(upper_key) - 1;
        }
        for (size_t k = 0; k < klen; k++) {
            char c = doc->extra_fields[f]->key[k];
            upper_key[k] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
        }
        upper_key[klen] = '\0';

        printf("%s%s%s\n", bold_on, upper_key, bold_off);
        print_man_indent(doc->extra_fields[f]->value, 4);
        printf("\n");
    }
}

static char *resolve_doc_file(const char *symbol, const char *explicit_file) {
    if (explicit_file) {
        return xstrdup(explicit_file);
    }

    if (symbol && *symbol) {
        char *path = module_resolve_path(symbol, NULL);
        if (path) {
            return path;
        }
    }

    /* Fallback for unqualified symbols such as Print -> std/io/io.em.  */
    if (strcmp(symbol, "Print") == 0) {
        if (access("std/io/io.em", R_OK) == 0) {
            return xstrdup("std/io/io.em");
        }
    }

    /* Fallback check for references/docmodule.em.  */
    if (access("references/docmodule.em", R_OK) == 0) {
        return xstrdup("references/docmodule.em");
    }

    return NULL;
}

int handle_man_command(int argc, char **argv) {
    const char *explicit_file = NULL;
    const char *symbol = NULL;
    bool json_mode = false;
    bool all_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            all_mode = true;
        } else if (strstr(argv[i], ".em") != NULL) {
            explicit_file = argv[i];
        } else if (argv[i][0] != '-') {
            if (!symbol) {
                symbol = argv[i];
            }
        }
    }

    if (!symbol && !explicit_file) {
        fprintf(stderr, "Usage: emc man [options] [file.em] <submodule>\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  emc man references/docmodule.em LibraryFunction\n");
        fprintf(stderr, "  emc man references/docmodule.em\n");
        fprintf(stderr, "  emc man std:io:Print\n");
        fprintf(stderr, "  emc man --json references/docmodule.em LibraryFunction\n");
        return 1;
    }

    char *file_path = resolve_doc_file(symbol ? symbol : "", explicit_file);
    if (!file_path) {
        fprintf(stderr,
                "Error: could not find documentation file for '%s'. Specify file path: emc man "
                "<file.em> %s\n",
                symbol ? symbol : "", symbol ? symbol : "");
        return 1;
    }

    char *source = read_source_file(file_path);
    if (!source) {
        fprintf(stderr, "Error: failed to read file '%s'\n", file_path);
        free(file_path);
        return 1;
    }

    Lexer lexer;
    lexer_init(&lexer, source);
    Parser parser;
    parser_init(&parser, &lexer);
    AstProgram *program = parser_parse(&parser);

    if (parser.had_error) {
        fprintf(stderr, "Error: failed to parse documentation file '%s'\n", file_path);
        ast_program_free(program);
        free(source);
        free(file_path);
        return 1;
    }

    const char *target_submod = symbol;
    if (symbol) {
        const char *last_colon = strrchr(symbol, ':');
        if (last_colon) {
            target_submod = last_colon + 1;
        }
    }

    size_t matched_count = 0;
    AstDocBlock *matched[64];

    for (size_t i = 0; i < program->doc_block_count; i++) {
        AstDocBlock *d = program->doc_blocks[i];
        bool match = false;
        if (!target_submod || all_mode) {
            match = true;
        } else if (strcmp(d->target_name, target_submod) == 0 ||
                   strcmp(d->target_name, symbol) == 0) {
            match = true;
        }

        if (match && matched_count < sizeof(matched) / sizeof(matched[0])) {
            matched[matched_count++] = d;
        }
    }

    if (matched_count == 0) {
        for (size_t i = 0; i < program->extern_block_count; i++) {
            AstExternBlock *eb = program->extern_blocks[i];
            if (!target_submod || all_mode || strcmp(eb->name, target_submod) == 0 ||
                strcmp(eb->name, symbol) == 0) {
                if (json_mode) {
                    printf("  {\n");
                    printf("    \"module\": \"%s\",\n", program->module_name);
                    printf("    \"submodule\": \"%s\",\n", eb->name);
                    printf("    \"type\": \"foreign\",\n");
                    printf("    \"language\": \"%s\",\n", eb->language ? eb->language : "c");
                    printf("    \"signature\": \"![%s]", eb->name);
                    if (eb->param_count > 0) {
                        printf(" {");
                        for (size_t p = 0; p < eb->param_count; p++) {
                            if (p > 0) {
                                printf(", ");
                            }
                            printf("%s: %s", eb->params[p]->name,
                                   ast_type_to_string(eb->params[p]->type));
                        }
                        printf("}");
                    }
                    printf("\"\n  }\n");
                } else {
                    printf("NAME\n    %s - foreign (%s) submodule\n\n", eb->name,
                           eb->language ? eb->language : "c");
                    printf("SYNOPSIS\n    ![%s]", eb->name);
                    if (eb->param_count > 0) {
                        printf(" {");
                        for (size_t p = 0; p < eb->param_count; p++) {
                            if (p > 0) {
                                printf(", ");
                            }
                            printf("%s: %s", eb->params[p]->name,
                                   ast_type_to_string(eb->params[p]->type));
                        }
                        printf("}");
                    }
                    printf("\n\n");
                    if (eb->raw_code) {
                        printf("SOURCE\n");
                        print_man_indent(eb->raw_code, 4);
                        printf("\n");
                    }
                }
                ast_program_free(program);
                free(source);
                free(file_path);
                return 0;
            }
        }

        fprintf(stderr, "Documentation for submodule '%s' not found in '%s'.\n",
                symbol ? symbol : "(all)", file_path);
        if (program->doc_block_count > 0 || program->extern_block_count > 0) {
            fprintf(stderr, "Available submodules in file:\n");
            for (size_t i = 0; i < program->doc_block_count; i++) {
                fprintf(stderr, "  - #[%s]\n", program->doc_blocks[i]->target_name);
            }
            for (size_t i = 0; i < program->extern_block_count; i++) {
                fprintf(stderr, "  - ![%s]\n", program->extern_blocks[i]->name);
            }
        }
        ast_program_free(program);
        free(source);
        free(file_path);
        return 1;
    }

    if (json_mode) {
        if (matched_count == 1) {
            print_doc_json(program, matched[0]);
            printf("\n");
        } else {
            printf("[\n");
            for (size_t i = 0; i < matched_count; i++) {
                print_doc_json(program, matched[i]);
                if (i < matched_count - 1) {
                    printf(",");
                }
                printf("\n");
            }
            printf("]\n");
        }
    } else {
        for (size_t i = 0; i < matched_count; i++) {
            print_doc_man_page(program, matched[i]);
            if (i < matched_count - 1) {
                printf("---------------------------------------------------------------------------"
                       "-----\n\n");
            }
        }
    }

    ast_program_free(program);
    free(source);
    free(file_path);
    return 0;
}
