/* Recursive descent parser for the Emergent language grammar.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Loc tok_loc(const Token *tok) {
    return loc_make(tok->line, tok->column);
}

static void error_at(Parser *parser, Token *token, const char *message) {
    if (parser->panic_mode) {
        return;
    }
    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "[Line %d, column %d] Syntax error", token->line, token->column);

    if (token->type == TOK_EOF) {
        fprintf(stderr, " at end of file (EOF)");
    } else if (token->type == TOK_ERROR) {
        fprintf(stderr, " at invalid character '%.*s'", (int)token->length, token->start);
    } else {
        fprintf(stderr, " near '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
}

static void advance(Parser *parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = lexer_next_token(parser->lexer);
        if (parser->current.type != TOK_ERROR) {
            break;
        }
        error_at(parser, &parser->current, "Invalid token in token stream");
    }
}

static bool check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (!check(parser, type)) {
        return false;
    }
    advance(parser);
    return true;
}

static Token expect(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        Token prev = parser->current;
        advance(parser);
        return prev;
    }
    error_at(parser, &parser->current, message);
    return parser->current;
}

static void synchronize(Parser *parser) {
    parser->panic_mode = false;

    while (parser->current.type != TOK_EOF) {
        if (parser->previous.type == TOK_SEMICOLON) {
            return;
        }

        switch (parser->current.type) {
        case TOK_BLOCK_SUBMOD:
        case TOK_LBRACKET:
        case TOK_VAR:
        case TOK_IF:
        case TOK_RETURN:
        case TOK_MODULE:
            return;
        default:
            break;
        }

        advance(parser);
    }
}

static AstType *parse_type(Parser *parser) {
    /* Pointer types are recursive, so **int naturally builds two nodes.  */
    if (match(parser, TOK_ASTERISK)) {
        AstType *base = parse_type(parser);
        return ast_type_new_pointer(base);
    }

    if (match(parser, TOK_I8)) {
        return ast_type_new(TYPE_I8);
    }
    if (match(parser, TOK_I16)) {
        return ast_type_new(TYPE_I16);
    }
    if (match(parser, TOK_I32)) {
        return ast_type_new(TYPE_I32);
    }
    if (match(parser, TOK_I64)) {
        return ast_type_new(TYPE_I64);
    }
    if (match(parser, TOK_U8)) {
        return ast_type_new(TYPE_U8);
    }
    if (match(parser, TOK_U16)) {
        return ast_type_new(TYPE_U16);
    }
    if (match(parser, TOK_U32)) {
        return ast_type_new(TYPE_U32);
    }
    if (match(parser, TOK_U64)) {
        return ast_type_new(TYPE_U64);
    }
    if (match(parser, TOK_INT)) {
        return ast_type_new(TYPE_INT);
    }
    if (match(parser, TOK_UINT)) {
        return ast_type_new(TYPE_UINT);
    }
    if (match(parser, TOK_F32)) {
        return ast_type_new(TYPE_F32);
    }
    if (match(parser, TOK_F64)) {
        return ast_type_new(TYPE_F64);
    }
    if (match(parser, TOK_STR)) {
        return ast_type_new(TYPE_STR);
    }
    if (match(parser, TOK_BOOL)) {
        return ast_type_new(TYPE_BOOL);
    }
    if (match(parser, TOK_VOID)) {
        return ast_type_new(TYPE_VOID);
    }

    /* Preserve unknown identifiers for user-defined types.  */
    if (check(parser, TOK_IDENTIFIER)) {
        Token ident = expect(parser, TOK_IDENTIFIER, "Expected type name");
        char tmp[128];
        size_t len = ident.length < sizeof(tmp) - 1 ? ident.length : sizeof(tmp) - 1;
        memcpy(tmp, ident.start, len);
        tmp[len] = '\0';
        return ast_type_new_custom(tmp);
    }

    error_at(parser, &parser->current, "Expected type name (e.g. int, i32, str, *i32)");
    return ast_type_new(TYPE_I32);
}

static AstExpr *parse_expression(Parser *parser);

static AstExpr *parse_primary(Parser *parser) {
    Loc loc = tok_loc(&parser->current);

    if (match(parser, TOK_INT_LIT)) {
        Token tok = parser->previous;
        char tmp[64];
        size_t len = tok.length < sizeof(tmp) - 1 ? tok.length : sizeof(tmp) - 1;
        memcpy(tmp, tok.start, len);
        tmp[len] = '\0';
        int64_t val = (int64_t)strtoll(tmp, NULL, 0);
        return ast_expr_new_int(val, loc);
    }

    if (match(parser, TOK_FLOAT_LIT)) {
        Token tok = parser->previous;
        char tmp[64];
        size_t len = tok.length < sizeof(tmp) - 1 ? tok.length : sizeof(tmp) - 1;
        memcpy(tmp, tok.start, len);
        tmp[len] = '\0';
        double val = strtod(tmp, NULL);
        return ast_expr_new_float(val, loc);
    }

    if (match(parser, TOK_STRING_LIT)) {
        Token tok = parser->previous;
        return ast_expr_new_string(tok.start, tok.length, loc);
    }

    if (match(parser, TOK_TRUE)) {
        return ast_expr_new_bool(true, loc);
    }

    if (match(parser, TOK_FALSE)) {
        return ast_expr_new_bool(false, loc);
    }

    if (match(parser, TOK_NULL)) {
        return ast_expr_new_null(loc);
    }

    if (match(parser, TOK_IDENTIFIER)) {
        Token tok = parser->previous;
        return ast_expr_new_ident(tok.start, tok.length, loc);
    }

    /* MOD and SUB also act as namespace values in calls such as mod.add().  */
    if (match(parser, TOK_MOD)) {
        Token tok = parser->previous;
        return ast_expr_new_ident(tok.start, tok.length, loc);
    }

    if (match(parser, TOK_LPAREN)) {
        AstExpr *expr = parse_expression(parser);
        expect(parser, TOK_RPAREN, "Expected closing parenthesis ')'");
        return expr;
    }

    error_at(parser, &parser->current,
             "Expected expression (identifier, number, string, or parenthesis)");
    return NULL;
}

static AstExpr *parse_postfix(Parser *parser) {
    AstExpr *expr = parse_primary(parser);
    if (!expr) {
        return NULL;
    }

    for (;;) {
        if (match(parser, TOK_LPAREN)) {
            Loc loc = tok_loc(&parser->previous);

            AstExpr **args = NULL;
            size_t arg_count = 0;
            size_t arg_capacity = 0;

            if (!check(parser, TOK_RPAREN)) {
                do {
                    AstExpr *arg = parse_expression(parser);
                    if (arg) {
                        da_grow(args, arg_count, arg_capacity);
                        args[arg_count++] = arg;
                    }
                } while (match(parser, TOK_COMMA));
            }

            expect(parser, TOK_RPAREN, "Expected ')' after function arguments");
            expr = ast_expr_new_call(expr, args, arg_count, loc);
        } else if (match(parser, TOK_DOT)) {
            Loc loc = tok_loc(&parser->previous);
            Token member =
                expect(parser, TOK_IDENTIFIER, "Expected field or method name after '.'");
            expr = ast_expr_new_member(expr, member.start, member.length, loc);
        } else if (match(parser, TOK_COLON)) {
            Loc loc = tok_loc(&parser->previous);
            Token subpath =
                expect(parser, TOK_IDENTIFIER, "Expected identifier after ':' in module path");
            expr = ast_expr_new_path(expr, subpath.start, subpath.length, loc);
        } else {
            break;
        }
    }

    return expr;
}

static AstExpr *parse_unary(Parser *parser) {
    Loc loc = tok_loc(&parser->current);

    if (match(parser, TOK_AMPERSAND)) {
        AstExpr *operand = parse_unary(parser);
        return ast_expr_new_unary(TOK_AMPERSAND, operand, loc);
    }
    if (match(parser, TOK_ASTERISK)) {
        AstExpr *operand = parse_unary(parser);
        return ast_expr_new_unary(TOK_ASTERISK, operand, loc);
    }
    if (match(parser, TOK_NOT)) {
        AstExpr *operand = parse_unary(parser);
        return ast_expr_new_unary(TOK_NOT, operand, loc);
    }
    if (match(parser, TOK_MINUS)) {
        AstExpr *operand = parse_unary(parser);
        return ast_expr_new_unary(TOK_MINUS, operand, loc);
    }
    if (match(parser, TOK_PLUS)) {
        return parse_unary(parser);
    }

    return parse_postfix(parser);
}

static AstExpr *parse_multiplicative(Parser *parser) {
    AstExpr *expr = parse_unary(parser);
    while (match(parser, TOK_ASTERISK) || match(parser, TOK_SLASH) || match(parser, TOK_PERCENT)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_unary(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_additive(Parser *parser) {
    AstExpr *expr = parse_multiplicative(parser);
    while (match(parser, TOK_PLUS) || match(parser, TOK_MINUS)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_multiplicative(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_comparison(Parser *parser) {
    AstExpr *expr = parse_additive(parser);
    while (match(parser, TOK_LESS) || match(parser, TOK_LESS_EQUAL) || match(parser, TOK_GREATER) ||
           match(parser, TOK_GREATER_EQUAL)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_additive(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_equality(Parser *parser) {
    AstExpr *expr = parse_comparison(parser);
    while (match(parser, TOK_EQUAL) || match(parser, TOK_NOT_EQUAL)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_comparison(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_logical_and(Parser *parser) {
    AstExpr *expr = parse_equality(parser);
    while (match(parser, TOK_AND)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_equality(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_logical_or(Parser *parser) {
    AstExpr *expr = parse_logical_and(parser);
    while (match(parser, TOK_OR)) {
        TokenType op = parser->previous.type;
        Loc loc = tok_loc(&parser->previous);
        AstExpr *right = parse_logical_and(parser);
        expr = ast_expr_new_binary(op, expr, right, loc);
    }
    return expr;
}

static AstExpr *parse_expression(Parser *parser) {
    return parse_logical_or(parser);
}

static AstStmt *parse_statement(Parser *parser);

static AstStmt *parse_var_decl(Parser *parser) {
    Loc loc = tok_loc(&parser->previous);

    Token name_tok = expect(parser, TOK_IDENTIFIER, "Expected variable name after 'var'");
    expect(parser, TOK_COLON, "Expected ':' after variable name");
    AstType *type = parse_type(parser);

    AstExpr *init_expr = NULL;
    if (match(parser, TOK_ASSIGN)) {
        init_expr = parse_expression(parser);
    }

    expect(parser, TOK_SEMICOLON, "Expected ';' at end of variable declaration");
    return ast_stmt_new_var_decl(name_tok.start, name_tok.length, type, init_expr, loc);
}

static AstStmt *parse_if_stmt(Parser *parser) {
    Loc loc = tok_loc(&parser->previous);

    expect(parser, TOK_LPAREN, "Expected '(' after 'if'");
    AstExpr *cond = parse_expression(parser);
    expect(parser, TOK_RPAREN, "Expected ')' after 'if' condition");

    expect(parser, TOK_LBRACE, "Expected '{' before 'if' body");

    AstStmt **then_stmts = NULL;
    size_t then_count = 0;
    size_t then_cap = 0;

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        AstStmt *s = parse_statement(parser);
        if (s) {
            da_grow(then_stmts, then_count, then_cap);
            then_stmts[then_count++] = s;
        }
    }
    expect(parser, TOK_RBRACE, "Expected '}' at end of 'if' block");

    AstStmt **else_stmts = NULL;
    size_t else_count = 0;
    size_t else_cap = 0;

    if (match(parser, TOK_ELSE)) {
        if (match(parser, TOK_IF)) {
            /* Represent else-if as a single nested statement.  */
            AstStmt *else_if = parse_if_stmt(parser);
            else_stmts = xmalloc(sizeof(*else_stmts));
            else_stmts[0] = else_if;
            else_count = 1;
        } else {
            expect(parser, TOK_LBRACE, "Expected '{' before 'else' body");
            while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
                AstStmt *s = parse_statement(parser);
                if (s) {
                    da_grow(else_stmts, else_count, else_cap);
                    else_stmts[else_count++] = s;
                }
            }
            expect(parser, TOK_RBRACE, "Expected '}' at end of 'else' block");
        }
    }

    return ast_stmt_new_if(cond, then_stmts, then_count, else_stmts, else_count, loc);
}

static AstStmt *parse_return_stmt(Parser *parser) {
    Loc loc = tok_loc(&parser->previous);

    AstExpr *val = NULL;
    if (!check(parser, TOK_SEMICOLON)) {
        val = parse_expression(parser);
    }

    expect(parser, TOK_SEMICOLON, "Expected ';' after 'return'");
    return ast_stmt_new_return(val, loc);
}

static void parse_var_decls_into_block(Parser *parser, AstBlock *block) {
    Loc loc = tok_loc(&parser->previous);

    Token names[32];
    size_t name_count = 0;
    names[name_count++] = expect(parser, TOK_IDENTIFIER, "Expected variable name after 'var'");

    while (match(parser, TOK_COMMA)) {
        if (name_count < 32) {
            names[name_count++] =
                expect(parser, TOK_IDENTIFIER, "Expected variable name after ','");
        }
    }

    expect(parser, TOK_COLON, "Expected ':' after variable name");
    AstType *type = parse_type(parser);

    AstExpr *init_expr = NULL;
    if (match(parser, TOK_ASSIGN)) {
        init_expr = parse_expression(parser);
    }

    expect(parser, TOK_SEMICOLON, "Expected ';' at end of variable declaration");

    for (size_t i = 0; i < name_count; i++) {
        AstType *t = (i == name_count - 1) ? type : ast_type_clone(type);
        AstExpr *init = NULL;
        if (init_expr) {
            init = (i == 0) ? init_expr : ast_expr_clone(init_expr);
        }
        AstStmt *stmt = ast_stmt_new_var_decl(names[i].start, names[i].length, t, init, loc);
        ast_block_add_stmt(block, stmt);
    }
}

static void parse_statement_into_block(Parser *parser, AstBlock *block) {
    if (match(parser, TOK_VAR)) {
        parse_var_decls_into_block(parser, block);
        return;
    }
    if (match(parser, TOK_IF)) {
        AstStmt *stmt = parse_if_stmt(parser);
        if (stmt) {
            ast_block_add_stmt(block, stmt);
        }
        return;
    }
    if (match(parser, TOK_RETURN)) {
        AstStmt *stmt = parse_return_stmt(parser);
        if (stmt) {
            ast_block_add_stmt(block, stmt);
        }
        return;
    }

    Loc loc = tok_loc(&parser->current);
    AstExpr *expr = parse_expression(parser);

    if (!expr) {
        synchronize(parser);
        return;
    }

    /* Each assignment owns its value subtree, hence the clones below.  */
    if (match(parser, TOK_COMMA)) {
        AstExpr *targets[32];
        size_t target_count = 0;
        targets[target_count++] = expr;

        do {
            if (target_count < 32) {
                targets[target_count++] = parse_primary(parser);
            }
        } while (match(parser, TOK_COMMA));

        expect(parser, TOK_ASSIGN, "Expected '=' after variable list");
        AstExpr *val = parse_expression(parser);
        expect(parser, TOK_SEMICOLON, "Expected ';' after assignment expression");

        for (size_t i = 0; i < target_count; i++) {
            AstExpr *v = (i == target_count - 1) ? val : ast_expr_clone(val);
            ast_block_add_stmt(block, ast_stmt_new_assign(targets[i], v, loc));
        }
        return;
    }

    if (match(parser, TOK_FLOW_RIGHT)) {
        Token dst;
        if (match(parser, TOK_NULL)) {
            dst = parser->previous;
        } else {
            dst = expect(parser, TOK_IDENTIFIER,
                         "Expected destination identifier or 'Null' after '>>'");
        }
        expect(parser, TOK_SEMICOLON, "Expected ';' after move operation '>>'");

        ast_block_add_stmt(block, ast_stmt_new_move(expr, dst.start, dst.length, loc));
        return;
    }

    if (match(parser, TOK_ASSIGN)) {
        AstExpr *val = parse_expression(parser);
        expect(parser, TOK_SEMICOLON, "Expected ';' after assignment expression");
        ast_block_add_stmt(block, ast_stmt_new_assign(expr, val, loc));
        return;
    }

    expect(parser, TOK_SEMICOLON, "Expected ';' at end of statement");
    ast_block_add_stmt(block, ast_stmt_new_expr(expr, loc));
}

static AstStmt *parse_statement(Parser *parser) {
    if (match(parser, TOK_VAR)) {
        return parse_var_decl(parser);
    }
    if (match(parser, TOK_IF)) {
        return parse_if_stmt(parser);
    }
    if (match(parser, TOK_RETURN)) {
        return parse_return_stmt(parser);
    }

    Loc loc = tok_loc(&parser->current);
    AstExpr *expr = parse_expression(parser);

    if (!expr) {
        synchronize(parser);
        return NULL;
    }

    if (match(parser, TOK_FLOW_RIGHT)) {
        Token dst;
        if (match(parser, TOK_NULL)) {
            dst = parser->previous;
        } else {
            dst = expect(parser, TOK_IDENTIFIER,
                         "Expected destination identifier or 'Null' after '>>'");
        }
        expect(parser, TOK_SEMICOLON, "Expected ';' after move operation '>>'");

        return ast_stmt_new_move(expr, dst.start, dst.length, loc);
    }

    if (match(parser, TOK_ASSIGN)) {
        AstExpr *val = parse_expression(parser);
        expect(parser, TOK_SEMICOLON, "Expected ';' after assignment expression");
        return ast_stmt_new_assign(expr, val, loc);
    }

    expect(parser, TOK_SEMICOLON, "Expected ';' at end of statement");
    return ast_stmt_new_expr(expr, loc);
}

static bool is_param_list_ahead(Parser *parser) {
    Lexer temp = *parser->lexer;
    Token t1 = lexer_next_token(&temp);
    if (t1.type == TOK_IDENTIFIER) {
        Token t2 = lexer_next_token(&temp);
        if (t2.type == TOK_COLON) {
            return true;
        }
    }
    return false;
}

static AstBlock *parse_block(Parser *parser) {
    Token name_tok = expect(parser, TOK_IDENTIFIER, "Expected block name (e.g. 'Init')");
    expect(parser, TOK_RBRACKET, "Expected ']' after block name");

    AstBlock *block = ast_block_new(name_tok.start, name_tok.length);

    /* A leading brace is ambiguous with the body, so peek for NAME:TYPE.  */
    if (match(parser, TOK_LPAREN)) {
        while (!check(parser, TOK_RPAREN) && !check(parser, TOK_EOF)) {
            Token p_name = expect(parser, TOK_IDENTIFIER, "Expected parameter name");
            expect(parser, TOK_COLON, "Expected ':' after parameter name");
            AstType *p_type = parse_type(parser);
            ast_block_add_param(block, p_name.start, p_name.length, p_type);
            if (!match(parser, TOK_COMMA)) {
                break;
            }
        }
        expect(parser, TOK_RPAREN, "Expected ')' after parameter list");
    } else if (check(parser, TOK_LBRACE) && is_param_list_ahead(parser)) {
        advance(parser);
        while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
            Token p_name = expect(parser, TOK_IDENTIFIER, "Expected parameter name");
            expect(parser, TOK_COLON, "Expected ':' after parameter name");
            AstType *p_type = parse_type(parser);
            ast_block_add_param(block, p_name.start, p_name.length, p_type);
            if (!match(parser, TOK_COMMA)) {
                break;
            }
        }
        expect(parser, TOK_RBRACE, "Expected '}' after parameter list");
    }

    if (match(parser, TOK_ARROW)) {
        block->ret_type = parse_type(parser);
    }

    if (match(parser, TOK_LBRACE)) {
        while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
            parse_statement_into_block(parser, block);
        }
        expect(parser, TOK_RBRACE, "Expected '}' at end of block");
    } else {
        while (!check(parser, TOK_BLOCK_SUBMOD) && !check(parser, TOK_BLOCK_DOC) &&
               !check(parser, TOK_BLOCK_EXTERN) && !check(parser, TOK_EOF)) {
            parse_statement_into_block(parser, block);
        }
    }

    return block;
}

static char *trim_whitespace(char *str) {
    assert(str);
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    return str;
}

static void parse_doc_arguments(AstDocBlock *doc, const char *raw) {
    char *copy = xstrdup(raw);
    char *saveptr1 = NULL;
    char *entry = strtok_r(copy, "\\", &saveptr1);
    while (entry) {
        char *trimmed = trim_whitespace(entry);
        if (*trimmed != '\0') {
            char *p1 = trimmed;
            char *semi1 = strchr(p1, ';');
            if (semi1) {
                *semi1 = '\0';
                char *p2 = semi1 + 1;
                char *semi2 = strchr(p2, ';');
                if (semi2) {
                    *semi2 = '\0';
                    char *p3 = semi2 + 1;
                    ast_doc_block_add_arg(doc, trim_whitespace(p1), trim_whitespace(p2),
                                          trim_whitespace(p3));
                } else {
                    ast_doc_block_add_arg(doc, "", trim_whitespace(p1), trim_whitespace(p2));
                }
            } else {
                ast_doc_block_add_arg(doc, "", "", trimmed);
            }
        }
        entry = strtok_r(NULL, "\\", &saveptr1);
    }
    free(copy);
}

static void parse_doc_returns(AstDocBlock *doc, const char *raw) {
    char *copy = xstrdup(raw);
    char *saveptr1 = NULL;
    char *entry = strtok_r(copy, "\\", &saveptr1);
    while (entry) {
        char *trimmed = trim_whitespace(entry);
        if (*trimmed != '\0') {
            char *p1 = trimmed;
            char *semi1 = strchr(p1, ';');
            if (semi1) {
                *semi1 = '\0';
                char *p2 = semi1 + 1;
                char *semi2 = strchr(p2, ';');
                if (semi2) {
                    *semi2 = '\0';
                    char *p3 = semi2 + 1;
                    ast_doc_block_add_return(doc, trim_whitespace(p1), trim_whitespace(p2),
                                             trim_whitespace(p3));
                } else {
                    ast_doc_block_add_return(doc, "", trim_whitespace(p1), trim_whitespace(p2));
                }
            } else {
                ast_doc_block_add_return(doc, "", "", trimmed);
            }
        }
        entry = strtok_r(NULL, "\\", &saveptr1);
    }
    free(copy);
}

static void parse_doc_description(AstDocBlock *doc, const char *raw) {
    char *copy = xstrdup(raw);
    char *saveptr1 = NULL;
    char *line = strtok_r(copy, "\\", &saveptr1);
    size_t total_len = 0;
    size_t capacity = 512;
    char *result = xmalloc(capacity);
    result[0] = '\0';

    while (line) {
        char *trimmed = trim_whitespace(line);
        if (*trimmed != '\0') {
            size_t tlen = strlen(trimmed);
            if (total_len + tlen + 2 >= capacity) {
                capacity = (total_len + tlen + 2) * 2;
                result = xrealloc(result, capacity);
            }
            if (total_len > 0) {
                result[total_len++] = '\n';
                result[total_len] = '\0';
            }
            memcpy(result + total_len, trimmed, tlen + 1);
            total_len += tlen;
        }
        line = strtok_r(NULL, "\\", &saveptr1);
    }
    ast_doc_block_set_description(doc, result);
    free(result);
    free(copy);
}

static AstDocBlock *parse_doc_block(Parser *parser) {
    Loc loc = tok_loc(&parser->previous);

    Token name_tok =
        expect(parser, TOK_IDENTIFIER, "Expected submodule name in documentation block #[Name]");
    char target_name[256];
    size_t len =
        name_tok.length < sizeof(target_name) - 1 ? name_tok.length : sizeof(target_name) - 1;
    memcpy(target_name, name_tok.start, len);
    target_name[len] = '\0';

    while (match(parser, TOK_COLON)) {
        Token sub = expect(parser, TOK_IDENTIFIER, "Expected path identifier after ':'");
        size_t cur_len = strlen(target_name);
        if (cur_len + 1 + sub.length < sizeof(target_name) - 1) {
            target_name[cur_len] = ':';
            memcpy(target_name + cur_len + 1, sub.start, sub.length);
            target_name[cur_len + 1 + sub.length] = '\0';
        }
    }

    expect(parser, TOK_RBRACKET, "Expected ']' after submodule name in documentation block");

    AstDocBlock *doc = ast_doc_block_new(target_name, strlen(target_name), loc);

    bool has_outer_brace = match(parser, TOK_LBRACE);

    while (true) {
        if (has_outer_brace && (check(parser, TOK_RBRACE) || check(parser, TOK_EOF))) {
            break;
        }
        if (!has_outer_brace && (check(parser, TOK_BLOCK_SUBMOD) || check(parser, TOK_BLOCK_DOC) ||
                                 check(parser, TOK_BLOCK_EXTERN) || check(parser, TOK_EOF))) {
            break;
        }

        if (!check(parser, TOK_IDENTIFIER)) {
            break;
        }

        Token field_tok = expect(parser, TOK_IDENTIFIER, "Expected documentation field name");
        char field_name[128];
        size_t flen =
            field_tok.length < sizeof(field_name) - 1 ? field_tok.length : sizeof(field_name) - 1;
        memcpy(field_name, field_tok.start, flen);
        field_name[flen] = '\0';

        expect(parser, TOK_COLON, "Expected ':' after documentation field name");

        if (!check(parser, TOK_LBRACE)) {
            error_at(parser, &parser->current, "Expected '{' with documentation field body");
            synchronize(parser);
            break;
        }

        char *raw_body = lexer_read_raw_braced(parser->lexer);
        advance(parser);

        expect(parser, TOK_SEMICOLON, "Expected ';' after documentation field");

        if (raw_body) {
            if (strcmp(field_name, "arguments") == 0) {
                parse_doc_arguments(doc, raw_body);
            } else if (strcmp(field_name, "returns") == 0) {
                parse_doc_returns(doc, raw_body);
            } else if (strcmp(field_name, "description") == 0) {
                parse_doc_description(doc, raw_body);
            } else {
                ast_doc_block_add_field(doc, field_name, raw_body);
            }
            free(raw_body);
        }
    }

    if (has_outer_brace) {
        expect(parser, TOK_RBRACE, "Expected '}' at end of documentation block");
    }

    return doc;
}

static AstExternBlock *parse_extern_block(Parser *parser) {
    Loc loc = tok_loc(&parser->previous);

    Token name_tok = expect(parser, TOK_IDENTIFIER, "Expected external submodule name after '!['");
    expect(parser, TOK_RBRACKET, "Expected ']' after external submodule name");

    AstExternBlock *eb = ast_extern_block_new(name_tok.start, name_tok.length, loc);

    if (match(parser, TOK_LPAREN)) {
        while (!check(parser, TOK_RPAREN) && !check(parser, TOK_EOF)) {
            Token p_name = expect(parser, TOK_IDENTIFIER, "Expected parameter name");
            expect(parser, TOK_COLON, "Expected ':' after parameter name");
            AstType *p_type = parse_type(parser);
            ast_extern_block_add_param(eb, p_name.start, p_name.length, p_type);
            if (!match(parser, TOK_COMMA)) {
                break;
            }
        }
        expect(parser, TOK_RPAREN, "Expected ')' after parameter list");
    } else if (check(parser, TOK_LBRACE) && is_param_list_ahead(parser)) {
        advance(parser);
        while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
            Token p_name = expect(parser, TOK_IDENTIFIER, "Expected parameter name");
            expect(parser, TOK_COLON, "Expected ':' after parameter name");
            AstType *p_type = parse_type(parser);
            ast_extern_block_add_param(eb, p_name.start, p_name.length, p_type);
            if (!match(parser, TOK_COMMA)) {
                break;
            }
        }
        expect(parser, TOK_RBRACE, "Expected '}' after parameter list");
    }

    if (match(parser, TOK_ARROW)) {
        eb->ret_type = parse_type(parser);
    }

    /* Foreign bodies may contain arbitrary tokens, so keep the source slice verbatim.  */
    if (check(parser, TOK_LBRACE)) {
        char *code = lexer_read_raw_braced(parser->lexer);
        advance(parser);
        ast_extern_block_set_code(eb, code);
        free(code);
    } else {
        const char *raw_start = parser->previous.start + parser->previous.length;
        parser->lexer->current = raw_start;
        char *code = lexer_read_raw_until_submodule_or_eof(parser->lexer);
        advance(parser);
        ast_extern_block_set_code(eb, code);
        free(code);
    }

    return eb;
}

void parser_init(Parser *parser, Lexer *lexer) {
    assert(parser && lexer);
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    advance(parser);
}

AstProgram *parser_parse(Parser *parser) {
    assert(parser);
    AstProgram *prog = NULL;

    if (match(parser, TOK_MODULE)) {
        Token mod_name = expect(parser, TOK_IDENTIFIER, "Expected module name after '--Module'");
        match(parser, TOK_SEMICOLON);

        char tmp[128];
        size_t len = mod_name.length < sizeof(tmp) - 1 ? mod_name.length : sizeof(tmp) - 1;
        memcpy(tmp, mod_name.start, len);
        tmp[len] = '\0';
        prog = ast_program_new(tmp);
    } else {
        prog = ast_program_new("main");
    }

    while (!check(parser, TOK_EOF)) {
        if (match(parser, TOK_BLOCK_SUBMOD)) {
            AstBlock *block = parse_block(parser);
            if (block) {
                ast_program_add_block(prog, block);
            }
        } else if (match(parser, TOK_BLOCK_DOC)) {
            AstDocBlock *doc = parse_doc_block(parser);
            if (doc) {
                ast_program_add_doc_block(prog, doc);
            }
        } else if (match(parser, TOK_BLOCK_EXTERN)) {
            AstExternBlock *eb = parse_extern_block(parser);
            if (eb) {
                ast_program_add_extern_block(prog, eb);
            }
        } else if (match(parser, TOK_LBRACKET)) {
            error_at(parser, &parser->previous,
                     "Submodule block must start with '-[' (e.g. -[Init])");
            synchronize(parser);
        } else {
            /* Preserve legacy top-level statements by wrapping them in Init.  */
            AstBlock *anon_block = ast_block_new("Init", 4);
            while (!check(parser, TOK_BLOCK_SUBMOD) && !check(parser, TOK_BLOCK_DOC) &&
                   !check(parser, TOK_BLOCK_EXTERN) && !check(parser, TOK_EOF)) {
                parse_statement_into_block(parser, anon_block);
            }
            ast_program_add_block(prog, anon_block);
        }
    }

    return prog;
}
