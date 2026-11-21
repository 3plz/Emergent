#ifndef EMC_LEXER_LEXER_H
#define EMC_LEXER_LEXER_H

#include "tokentypes.h"
#include <stddef.h>

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int column;
} Token;

typedef struct {
    const char *start;
    const char *current;
    int line;
    int column;
    int token_line;
    int token_column;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);
char *lexer_read_raw_braced(Lexer *lexer);
char *lexer_read_raw_until_submodule_or_eof(Lexer *lexer);

#endif
