#include "lexer.h"
#include "../common.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void lexer_init(Lexer *lexer, const char *source) {
    assert(lexer && source);
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_line = 1;
    lexer->token_column = 1;
}

static bool is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

static char peek(Lexer *lexer) {
    return *lexer->current;
}

static char peek_next(Lexer *lexer) {
    if (is_at_end(lexer)) {
        return '\0';
    }
    return lexer->current[1];
}

static char advance(Lexer *lexer) {
    if (is_at_end(lexer)) {
        return '\0';
    }
    char c = *lexer->current;
    lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static bool match(Lexer *lexer, char expected) {
    if (is_at_end(lexer) || *lexer->current != expected) {
        return false;
    }
    advance(lexer);
    return true;
}

static Token make_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->token_line;
    token.column = lexer->token_column;
    return token;
}

static void skip_whitespace_and_comments(Lexer *lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            advance(lexer);
            break;
        case '\n':
            advance(lexer);
            break;
        case '/':
            if (peek_next(lexer) == '/') {
                while (!is_at_end(lexer) && peek(lexer) != '\n') {
                    advance(lexer);
                }
            } else if (peek_next(lexer) == '*') {
                advance(lexer);
                advance(lexer);
                while (!is_at_end(lexer)) {
                    if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                        advance(lexer);
                        advance(lexer);
                        break;
                    }
                    if (peek(lexer) == '/' && peek_next(lexer) == '*') {
                        advance(lexer);
                        advance(lexer);
                        break;
                    }
                    advance(lexer);
                }
            } else {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static Token scan_string(Lexer *lexer) {
    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\\' && peek_next(lexer) != '\0') {
            advance(lexer);
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_ERROR);
    }

    advance(lexer);
    return make_token(lexer, TOK_STRING_LIT);
}

static Token scan_number(Lexer *lexer) {
    if (lexer->start[0] == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        advance(lexer);
        while (isxdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, TOK_INT_LIT);
    }

    while (isdigit((unsigned char)peek(lexer))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && isdigit((unsigned char)peek_next(lexer))) {
        advance(lexer);
        while (isdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, TOK_FLOAT_LIT);
    }

    return make_token(lexer, TOK_INT_LIT);
}

static TokenType check_keyword(Lexer *lexer) {
    size_t len = (size_t)(lexer->current - lexer->start);
    const char *text = lexer->start;

#define MATCH_KW(str, token_type)                                                                  \
    if (len == strlen(str) && memcmp(text, str, len) == 0)                                         \
        return token_type;

    MATCH_KW("var", TOK_VAR);
    MATCH_KW("immut", TOK_IMMUT);
    MATCH_KW("const", TOK_CONST);
    MATCH_KW("as", TOK_AS);
    MATCH_KW("if", TOK_IF);
    MATCH_KW("else", TOK_ELSE);
    MATCH_KW("for", TOK_FOR);
    MATCH_KW("while", TOK_WHILE);
    MATCH_KW("return", TOK_RETURN);
    MATCH_KW("mod", TOK_MOD);
    MATCH_KW("MODULE", TOK_MODULE);
    MATCH_KW("Module", TOK_MODULE);
    MATCH_KW("module", TOK_MODULE);
    MATCH_KW("true", TOK_TRUE);
    MATCH_KW("false", TOK_FALSE);
    MATCH_KW("null", TOK_NULL);
    MATCH_KW("Null", TOK_NULL);
    MATCH_KW("NULL", TOK_NULL);

    MATCH_KW("i8", TOK_I8);
    MATCH_KW("i16", TOK_I16);
    MATCH_KW("i32", TOK_I32);
    MATCH_KW("i64", TOK_I64);
    MATCH_KW("u8", TOK_U8);
    MATCH_KW("u16", TOK_U16);
    MATCH_KW("u32", TOK_U32);
    MATCH_KW("u64", TOK_U64);
    MATCH_KW("int", TOK_INT);
    MATCH_KW("uint", TOK_UINT);
    MATCH_KW("f32", TOK_F32);
    MATCH_KW("f64", TOK_F64);
    MATCH_KW("str", TOK_STR);
    MATCH_KW("bool", TOK_BOOL);
    MATCH_KW("void", TOK_VOID);
    MATCH_KW("array", TOK_ARRAY);

#undef MATCH_KW

    return TOK_IDENTIFIER;
}

static Token scan_identifier_or_keyword(Lexer *lexer) {
    while (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    return make_token(lexer, check_keyword(lexer));
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace_and_comments(lexer);

    lexer->start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF);
    }

    char c = advance(lexer);

    if (c == '"') {
        return scan_string(lexer);
    }

    if (isdigit((unsigned char)c)) {
        return scan_number(lexer);
    }

    if (isalpha((unsigned char)c) || c == '_') {
        return scan_identifier_or_keyword(lexer);
    }

    switch (c) {
    case '(':
        return make_token(lexer, TOK_LPAREN);
    case ')':
        return make_token(lexer, TOK_RPAREN);
    case '{':
        return make_token(lexer, TOK_LBRACE);
    case '}':
        return make_token(lexer, TOK_RBRACE);
    case '[':
        return make_token(lexer, TOK_LBRACKET);
    case ']':
        return make_token(lexer, TOK_RBRACKET);
    case ',':
        return make_token(lexer, TOK_COMMA);
    case ';':
        return make_token(lexer, TOK_SEMICOLON);
    case ':':
        return make_token(lexer, TOK_COLON);
    case '.':
        return make_token(lexer, TOK_DOT);
    case '%':
        return make_token(lexer, TOK_PERCENT);

    case '+':
        if (match(lexer, '+')) {
            return make_token(lexer, TOK_INCREMENT);
        }
        return make_token(lexer, TOK_PLUS);

    case '-':
        if (match(lexer, '>')) {
            return make_token(lexer, TOK_ARROW);
        }
        if (match(lexer, '[')) {
            return make_token(lexer, TOK_BLOCK_SUBMOD);
        }
        if (match(lexer, '-')) {
            if (strncasecmp(lexer->current, "MODULE", 6) == 0) {
                for (int i = 0; i < 6; i++) {
                    advance(lexer);
                }
                return make_token(lexer, TOK_MODULE);
            }
            return make_token(lexer, TOK_DECREMENT);
        }
        return make_token(lexer, TOK_MINUS);

    case '*':
        return make_token(lexer, TOK_ASTERISK);
    case '/':
        return make_token(lexer, TOK_SLASH);

    case '=':
        if (match(lexer, '=')) {
            return make_token(lexer, TOK_EQUAL);
        }
        return make_token(lexer, TOK_ASSIGN);

    case '!':
        if (match(lexer, '[')) {
            return make_token(lexer, TOK_BLOCK_EXTERN);
        }
        if (match(lexer, '=')) {
            return make_token(lexer, TOK_NOT_EQUAL);
        }
        return make_token(lexer, TOK_NOT);

    case '>':
        if (match(lexer, '>')) {
            return make_token(lexer, TOK_FLOW_RIGHT);
        }
        if (match(lexer, '=')) {
            return make_token(lexer, TOK_GREATER_EQUAL);
        }
        return make_token(lexer, TOK_GREATER);

    case '<':
        if (match(lexer, '<')) {
            return make_token(lexer, TOK_FLOW_LEFT);
        }
        if (match(lexer, '=')) {
            return make_token(lexer, TOK_LESS_EQUAL);
        }
        return make_token(lexer, TOK_LESS);

    case '#':
        if (match(lexer, '[')) {
            return make_token(lexer, TOK_BLOCK_DOC);
        }
        return make_token(lexer, TOK_HASH);

    case '&':
        if (match(lexer, '&')) {
            return make_token(lexer, TOK_AND);
        }
        return make_token(lexer, TOK_AMPERSAND);

    case '|':
        if (match(lexer, '|')) {
            return make_token(lexer, TOK_OR);
        }
        return make_token(lexer, TOK_ERROR);
    }

    return make_token(lexer, TOK_ERROR);
}

char *lexer_read_raw_braced(Lexer *lexer) {
    assert(lexer);
    const char *start = lexer->current;
    int brace_depth = 1;

    while (!is_at_end(lexer) && brace_depth > 0) {
        char c = *lexer->current;
        if (c == '\n') {
            lexer->line++;
            lexer->column = 1;
            lexer->current++;
        } else if (c == '{') {
            brace_depth++;
            lexer->column++;
            lexer->current++;
        } else if (c == '}') {
            brace_depth--;
            if (brace_depth == 0) {
                break;
            }
            lexer->column++;
            lexer->current++;
        } else {
            lexer->column++;
            lexer->current++;
        }
    }

    size_t len = (size_t)(lexer->current - start);
    char *result = xmalloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';

    if (!is_at_end(lexer) && *lexer->current == '}') {
        lexer->column++;
        lexer->current++;
    }

    return result;
}

char *lexer_read_raw_until_submodule_or_eof(Lexer *lexer) {
    assert(lexer);
    const char *start = lexer->current;
    const char *p = start;

    while (*p != '\0') {
        if (p == start || *(p - 1) == '\n') {
            const char *check = p;
            while (*check == ' ' || *check == '\t') {
                check++;
            }
            if ((check[0] == '-' && check[1] == '[') || (check[0] == '#' && check[1] == '[') ||
                (check[0] == '!' && check[1] == '[') ||
                (check[0] == '-' && check[1] == '-' && strncasecmp(check + 2, "MODULE", 6) == 0)) {
                break;
            }
        }

        if (*p == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        p++;
    }

    size_t len = (size_t)(p - start);
    char *result = xmalloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';

    lexer->current = p;
    return result;
}
