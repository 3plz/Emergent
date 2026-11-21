#ifndef EMC_PARSER_AST_H
#define EMC_PARSER_AST_H

#include "../common.h"
#include "../lexer/tokentypes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TYPE_VOID,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_INT,
    TYPE_UINT,
    TYPE_F32,
    TYPE_F64,
    TYPE_STR,
    TYPE_BOOL,
    TYPE_POINTER,
    TYPE_CUSTOM
} AstTypeKind;

typedef struct AstType {
    AstTypeKind kind;
    struct AstType *base_type;
    char *custom_name;
} AstType;

AstType *ast_type_new(AstTypeKind kind);
AstType *ast_type_new_pointer(AstType *base_type);
AstType *ast_type_new_custom(const char *name);
void ast_type_free(AstType *type);
const char *ast_type_to_string(const AstType *type);

typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_STRING_LIT,
    EXPR_BOOL_LIT,
    EXPR_NULL_LIT,
    EXPR_IDENTIFIER,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_CALL,
    EXPR_MEMBER,
    EXPR_PATH
} AstExprKind;

typedef struct AstExpr AstExpr;

typedef struct {
    TokenType op;
    AstExpr *left;
    AstExpr *right;
} AstBinaryExpr;

typedef struct {
    TokenType op;
    AstExpr *operand;
} AstUnaryExpr;

typedef struct {
    AstExpr *callee;
    AstExpr **args;
    size_t arg_count;
} AstCallExpr;

typedef struct {
    AstExpr *target;
    char *member;
} AstMemberExpr;

typedef struct {
    AstExpr *target;
    char *subpath;
} AstPathExpr;

struct AstExpr {
    AstExprKind kind;
    Loc loc;
    union {
        int64_t int_val;
        double float_val;
        char *str_val;
        bool bool_val;
        char *ident_name;
        AstBinaryExpr binary;
        AstUnaryExpr unary;
        AstCallExpr call;
        AstMemberExpr member;
        AstPathExpr path;
    };
};

AstExpr *ast_expr_new_int(int64_t val, Loc loc);
AstExpr *ast_expr_new_float(double val, Loc loc);
AstExpr *ast_expr_new_string(const char *val, size_t len, Loc loc);
AstExpr *ast_expr_new_bool(bool val, Loc loc);
AstExpr *ast_expr_new_null(Loc loc);
AstExpr *ast_expr_new_ident(const char *name, size_t len, Loc loc);
AstExpr *ast_expr_new_binary(TokenType op, AstExpr *left, AstExpr *right, Loc loc);
AstExpr *ast_expr_new_unary(TokenType op, AstExpr *operand, Loc loc);
AstExpr *ast_expr_new_call(AstExpr *callee, AstExpr **args, size_t arg_count, Loc loc);
AstExpr *ast_expr_new_member(AstExpr *target, const char *member, size_t len, Loc loc);
AstExpr *ast_expr_new_path(AstExpr *target, const char *subpath, size_t len, Loc loc);
void ast_expr_free(AstExpr *expr);

typedef enum {
    STMT_VAR_DECL,
    STMT_ASSIGN,
    STMT_MOVE,
    STMT_IF,
    STMT_RETURN,
    STMT_EXPR
} AstStmtKind;

typedef struct AstStmt AstStmt;

typedef struct {
    char *name;
    AstType *type;
    AstExpr *init_expr;
} AstVarDeclStmt;

typedef struct {
    AstExpr *target;
    AstExpr *value;
} AstAssignStmt;

typedef struct {
    AstExpr *src_expr;
    char *dst;
} AstMoveStmt;

typedef struct {
    AstExpr *cond;
    AstStmt **then_stmts;
    size_t then_count;
    AstStmt **else_stmts;
    size_t else_count;
} AstIfStmt;

typedef struct {
    AstExpr *expr;
} AstReturnStmt;

struct AstStmt {
    AstStmtKind kind;
    Loc loc;
    union {
        AstVarDeclStmt var_decl;
        AstAssignStmt assign;
        AstMoveStmt move;
        AstIfStmt if_stmt;
        AstReturnStmt return_stmt;
        AstExpr *expr_stmt;
    };
};

AstStmt *ast_stmt_new_var_decl(const char *name, size_t len, AstType *type, AstExpr *init, Loc loc);
AstStmt *ast_stmt_new_assign(AstExpr *target, AstExpr *val, Loc loc);
AstStmt *ast_stmt_new_move(AstExpr *src_expr, const char *dst, size_t dst_len, Loc loc);
AstStmt *ast_stmt_new_if(AstExpr *cond, AstStmt **then_stmts, size_t then_count,
                         AstStmt **else_stmts, size_t else_count, Loc loc);
AstStmt *ast_stmt_new_return(AstExpr *val, Loc loc);
AstStmt *ast_stmt_new_expr(AstExpr *expr, Loc loc);
void ast_stmt_free(AstStmt *stmt);

typedef struct {
    char *name;
    AstType *type;
} AstParam;

AstParam *ast_param_new(const char *name, size_t len, AstType *type);
void ast_param_free(AstParam *param);

typedef struct {
    char *name;
    char *type;
    char *desc;
} AstDocArg;

typedef struct {
    char *name;
    char *type;
    char *desc;
} AstDocReturn;

typedef struct {
    char *key;
    char *value;
} AstDocField;

typedef struct {
    char *target_name;
    AstDocArg **args;
    size_t arg_count;
    size_t arg_capacity;
    AstDocReturn **returns;
    size_t return_count;
    size_t return_capacity;
    char *description;
    AstDocField **extra_fields;
    size_t extra_field_count;
    size_t extra_field_capacity;
    Loc loc;
} AstDocBlock;

AstDocBlock *ast_doc_block_new(const char *target_name, size_t len, Loc loc);
void ast_doc_block_add_arg(AstDocBlock *doc, const char *name, const char *type, const char *desc);
void ast_doc_block_add_return(AstDocBlock *doc, const char *name, const char *type,
                              const char *desc);
void ast_doc_block_set_description(AstDocBlock *doc, const char *desc);
void ast_doc_block_add_field(AstDocBlock *doc, const char *key, const char *value);
void ast_doc_block_free(AstDocBlock *doc);

typedef struct {
    char *name;
    char *language;
    char *module_name;
    AstParam **params;
    size_t param_count;
    size_t param_capacity;
    AstType *ret_type;
    char *raw_code;
    Loc loc;
} AstExternBlock;

AstExternBlock *ast_extern_block_new(const char *name, size_t len, Loc loc);
void ast_extern_block_add_param(AstExternBlock *block, const char *name, size_t len, AstType *type);
void ast_extern_block_set_code(AstExternBlock *block, const char *code);
void ast_extern_block_free(AstExternBlock *block);

typedef struct {
    char *name;
    char *module_name;
    AstType *ret_type;
    AstParam **params;
    size_t param_count;
    size_t param_capacity;
    AstStmt **stmts;
    size_t stmt_count;
    size_t stmt_capacity;
} AstBlock;

AstBlock *ast_block_new(const char *name, size_t len);
void ast_block_add_param(AstBlock *block, const char *name, size_t len, AstType *type);
void ast_block_add_stmt(AstBlock *block, AstStmt *stmt);
void ast_block_free(AstBlock *block);

typedef struct {
    char *module_name;
    AstBlock **blocks;
    size_t block_count;
    size_t block_capacity;
    AstDocBlock **doc_blocks;
    size_t doc_block_count;
    size_t doc_block_capacity;
    AstExternBlock **extern_blocks;
    size_t extern_block_count;
    size_t extern_block_capacity;
    char **link_libs;
    size_t link_lib_count;
    size_t link_lib_capacity;
} AstProgram;

AstProgram *ast_program_new(const char *module_name);
void ast_program_add_block(AstProgram *program, AstBlock *block);
void ast_program_add_doc_block(AstProgram *program, AstDocBlock *doc);
void ast_program_add_extern_block(AstProgram *program, AstExternBlock *block);
void ast_program_add_link_lib(AstProgram *program, const char *lib);
void ast_program_collect_link_libs(AstProgram *program);
void ast_program_free(AstProgram *program);

AstType *ast_type_clone(const AstType *t);
AstExpr *ast_expr_clone(const AstExpr *e);

void ast_print(const AstProgram *program);

#endif
