#ifndef TOKENTYPES_H
#define TOKENTYPES_H

typedef enum {
    /* Operators.  */
    TOK_PLUS,
    TOK_MINUS,
    TOK_ASTERISK,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_INCREMENT,
    TOK_DECREMENT,
    TOK_ASSIGN,
    TOK_EQUAL,
    TOK_NOT_EQUAL,
    TOK_GREATER,
    TOK_GREATER_EQUAL,
    TOK_LESS,
    TOK_LESS_EQUAL,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_FLOW_RIGHT,
    TOK_FLOW_LEFT,
    TOK_ARROW,
    TOK_AMPERSAND,

    /* Block markers and punctuation.  */
    TOK_BLOCK_SUBMOD,
    TOK_BLOCK_DOC,
    TOK_BLOCK_EXTERN,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_HASH,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_DOT,

    TOK_IDENTIFIER,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,

    /* Built-in types.  */
    TOK_I8,
    TOK_I16,
    TOK_I32,
    TOK_I64,
    TOK_U8,
    TOK_U16,
    TOK_U32,
    TOK_U64,
    TOK_INT,
    TOK_UINT,
    TOK_F32,
    TOK_F64,
    TOK_STR,
    TOK_BOOL,
    TOK_VOID,
    TOK_ARRAY,

    /* Keywords.  */
    TOK_VAR,
    TOK_IMMUT,
    TOK_CONST,
    TOK_AS,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,
    TOK_RETURN,
    TOK_MOD,
    TOK_MODULE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,

    TOK_EOF,
    TOK_ERROR
} TokenType;

static inline const char *token_type_name(TokenType type) {
    switch (type) {
    case TOK_PLUS:
        return "+";
    case TOK_MINUS:
        return "-";
    case TOK_ASTERISK:
        return "*";
    case TOK_SLASH:
        return "/";
    case TOK_PERCENT:
        return "%";
    case TOK_INCREMENT:
        return "++";
    case TOK_DECREMENT:
        return "--";

    case TOK_ASSIGN:
        return "=";

    case TOK_EQUAL:
        return "==";
    case TOK_NOT_EQUAL:
        return "!=";
    case TOK_GREATER:
        return ">";
    case TOK_GREATER_EQUAL:
        return ">=";
    case TOK_LESS:
        return "<";
    case TOK_LESS_EQUAL:
        return "<=";

    case TOK_AND:
        return "&&";
    case TOK_OR:
        return "||";
    case TOK_NOT:
        return "!";

    case TOK_FLOW_RIGHT:
        return ">>";
    case TOK_FLOW_LEFT:
        return "<<";

    case TOK_ARROW:
        return "->";

    case TOK_AMPERSAND:
        return "&";

    case TOK_BLOCK_SUBMOD:
        return "-[";
    case TOK_BLOCK_DOC:
        return "#[";
    case TOK_BLOCK_EXTERN:
        return "![";

    case TOK_LPAREN:
        return "(";
    case TOK_RPAREN:
        return ")";
    case TOK_LBRACE:
        return "{";
    case TOK_RBRACE:
        return "}";
    case TOK_LBRACKET:
        return "[";
    case TOK_RBRACKET:
        return "]";

    case TOK_HASH:
        return "#";
    case TOK_COMMA:
        return ",";
    case TOK_SEMICOLON:
        return ";";
    case TOK_COLON:
        return ":";
    case TOK_DOT:
        return ".";

    case TOK_IDENTIFIER:
        return "identifier";
    case TOK_INT_LIT:
        return "int literal";
    case TOK_FLOAT_LIT:
        return "float literal";
    case TOK_STRING_LIT:
        return "string literal";

    case TOK_I8:
        return "i8";
    case TOK_I16:
        return "i16";
    case TOK_I32:
        return "i32";
    case TOK_I64:
        return "i64";
    case TOK_U8:
        return "u8";
    case TOK_U16:
        return "u16";
    case TOK_U32:
        return "u32";
    case TOK_U64:
        return "u64";
    case TOK_INT:
        return "int";
    case TOK_UINT:
        return "uint";
    case TOK_F32:
        return "f32";
    case TOK_F64:
        return "f64";
    case TOK_STR:
        return "str";
    case TOK_BOOL:
        return "bool";
    case TOK_VOID:
        return "void";
    case TOK_ARRAY:
        return "array";

    case TOK_VAR:
        return "var";
    case TOK_IMMUT:
        return "immut";
    case TOK_CONST:
        return "const";
    case TOK_AS:
        return "as";
    case TOK_IF:
        return "if";
    case TOK_ELSE:
        return "else";
    case TOK_FOR:
        return "for";
    case TOK_WHILE:
        return "while";
    case TOK_RETURN:
        return "return";
    case TOK_MOD:
        return "mod";
    case TOK_MODULE:
        return "MODULE";
    case TOK_TRUE:
        return "true";
    case TOK_FALSE:
        return "false";
    case TOK_NULL:
        return "null";

    case TOK_EOF:
        return "EOF";
    case TOK_ERROR:
        return "ERROR";

    default:
        return "UNKNOWN";
    }
}
#endif
