#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Types --- */
AstType *ast_type_new(AstTypeKind kind) {
    AstType *t = xcalloc(1, sizeof(*t));
    t->kind = kind;
    return t;
}

AstType *ast_type_new_pointer(AstType *base_type) {
    AstType *t = xcalloc(1, sizeof(*t));
    t->kind = TYPE_POINTER;
    t->base_type = base_type;
    return t;
}

AstType *ast_type_new_custom(const char *name) {
    AstType *t = xcalloc(1, sizeof(*t));
    t->kind = TYPE_CUSTOM;
    t->custom_name = name ? xstrdup(name) : NULL;
    return t;
}

void ast_type_free(AstType *type) {
    if (!type) {
        return;
    }
    if (type->base_type) {
        ast_type_free(type->base_type);
    }
    if (type->custom_name) {
        free(type->custom_name);
    }
    free(type);
}

const char *ast_type_to_string(const AstType *type) {
    if (!type) {
        return "unknown";
    }
    switch (type->kind) {
    case TYPE_VOID:
        return "void";
    case TYPE_I8:
        return "i8";
    case TYPE_I16:
        return "i16";
    case TYPE_I32:
        return "i32";
    case TYPE_I64:
        return "i64";
    case TYPE_U8:
        return "u8";
    case TYPE_U16:
        return "u16";
    case TYPE_U32:
        return "u32";
    case TYPE_U64:
        return "u64";
    case TYPE_INT:
        return "int";
    case TYPE_UINT:
        return "uint";
    case TYPE_F32:
        return "f32";
    case TYPE_F64:
        return "f64";
    case TYPE_STR:
        return "str";
    case TYPE_BOOL:
        return "bool";
    case TYPE_POINTER:
        return "pointer";
    case TYPE_CUSTOM:
        return type->custom_name ? type->custom_name : "custom";
    }
    return "unknown";
}

/* --- Expressions --- */
static AstExpr *alloc_expr(AstExprKind kind, Loc loc) {
    AstExpr *e = xcalloc(1, sizeof(*e));
    e->kind = kind;
    e->loc = loc;
    return e;
}

AstExpr *ast_expr_new_int(int64_t val, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_INT_LIT, loc);
    e->int_val = val;
    return e;
}

AstExpr *ast_expr_new_float(double val, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_FLOAT_LIT, loc);
    e->float_val = val;
    return e;
}

AstExpr *ast_expr_new_string(const char *val, size_t len, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_STRING_LIT, loc);
    if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
        e->str_val = xstrndup(val + 1, len - 2);
    } else {
        e->str_val = xstrndup(val, len);
    }
    return e;
}

AstExpr *ast_expr_new_bool(bool val, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_BOOL_LIT, loc);
    e->bool_val = val;
    return e;
}

AstExpr *ast_expr_new_null(Loc loc) {
    return alloc_expr(EXPR_NULL_LIT, loc);
}

AstExpr *ast_expr_new_ident(const char *name, size_t len, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_IDENTIFIER, loc);
    e->ident_name = xstrndup(name, len);
    return e;
}

AstExpr *ast_expr_new_binary(TokenType op, AstExpr *left, AstExpr *right, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_BINARY, loc);
    e->binary.op = op;
    e->binary.left = left;
    e->binary.right = right;
    return e;
}

AstExpr *ast_expr_new_unary(TokenType op, AstExpr *operand, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_UNARY, loc);
    e->unary.op = op;
    e->unary.operand = operand;
    return e;
}

AstExpr *ast_expr_new_call(AstExpr *callee, AstExpr **args, size_t arg_count, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_CALL, loc);
    e->call.callee = callee;
    e->call.args = args;
    e->call.arg_count = arg_count;
    return e;
}

AstExpr *ast_expr_new_member(AstExpr *target, const char *member, size_t len, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_MEMBER, loc);
    e->member.target = target;
    e->member.member = xstrndup(member, len);
    return e;
}

AstExpr *ast_expr_new_path(AstExpr *target, const char *subpath, size_t len, Loc loc) {
    AstExpr *e = alloc_expr(EXPR_PATH, loc);
    e->path.target = target;
    e->path.subpath = xstrndup(subpath, len);
    return e;
}

void ast_expr_free(AstExpr *expr) {
    if (!expr) {
        return;
    }
    switch (expr->kind) {
    case EXPR_STRING_LIT:
        free(expr->str_val);
        break;
    case EXPR_IDENTIFIER:
        free(expr->ident_name);
        break;
    case EXPR_BINARY:
        ast_expr_free(expr->binary.left);
        ast_expr_free(expr->binary.right);
        break;
    case EXPR_UNARY:
        ast_expr_free(expr->unary.operand);
        break;
    case EXPR_CALL:
        ast_expr_free(expr->call.callee);
        for (size_t i = 0; i < expr->call.arg_count; i++) {
            ast_expr_free(expr->call.args[i]);
        }
        free(expr->call.args);
        break;
    case EXPR_MEMBER:
        ast_expr_free(expr->member.target);
        free(expr->member.member);
        break;
    case EXPR_PATH:
        ast_expr_free(expr->path.target);
        free(expr->path.subpath);
        break;
    default:
        break;
    }
    free(expr);
}

/* --- Statements --- */
static AstStmt *alloc_stmt(AstStmtKind kind, Loc loc) {
    AstStmt *s = xcalloc(1, sizeof(*s));
    s->kind = kind;
    s->loc = loc;
    return s;
}

AstStmt *ast_stmt_new_var_decl(const char *name, size_t len, AstType *type, AstExpr *init,
                               Loc loc) {
    AstStmt *s = alloc_stmt(STMT_VAR_DECL, loc);
    s->var_decl.name = xstrndup(name, len);
    s->var_decl.type = type;
    s->var_decl.init_expr = init;
    return s;
}

AstStmt *ast_stmt_new_assign(AstExpr *target, AstExpr *val, Loc loc) {
    AstStmt *s = alloc_stmt(STMT_ASSIGN, loc);
    s->assign.target = target;
    s->assign.value = val;
    return s;
}

AstStmt *ast_stmt_new_move(AstExpr *src_expr, const char *dst, size_t dst_len, Loc loc) {
    AstStmt *s = alloc_stmt(STMT_MOVE, loc);
    s->move.src_expr = src_expr;
    s->move.dst = xstrndup(dst, dst_len);
    return s;
}

AstStmt *ast_stmt_new_if(AstExpr *cond, AstStmt **then_stmts, size_t then_count,
                         AstStmt **else_stmts, size_t else_count, Loc loc) {
    AstStmt *s = alloc_stmt(STMT_IF, loc);
    s->if_stmt.cond = cond;
    s->if_stmt.then_stmts = then_stmts;
    s->if_stmt.then_count = then_count;
    s->if_stmt.else_stmts = else_stmts;
    s->if_stmt.else_count = else_count;
    return s;
}

AstStmt *ast_stmt_new_return(AstExpr *val, Loc loc) {
    AstStmt *s = alloc_stmt(STMT_RETURN, loc);
    s->return_stmt.expr = val;
    return s;
}

AstStmt *ast_stmt_new_expr(AstExpr *expr, Loc loc) {
    AstStmt *s = alloc_stmt(STMT_EXPR, loc);
    s->expr_stmt = expr;
    return s;
}

void ast_stmt_free(AstStmt *stmt) {
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_VAR_DECL:
        free(stmt->var_decl.name);
        ast_type_free(stmt->var_decl.type);
        if (stmt->var_decl.init_expr) {
            ast_expr_free(stmt->var_decl.init_expr);
        }
        break;
    case STMT_ASSIGN:
        ast_expr_free(stmt->assign.target);
        ast_expr_free(stmt->assign.value);
        break;
    case STMT_MOVE:
        if (stmt->move.src_expr) {
            ast_expr_free(stmt->move.src_expr);
        }
        free(stmt->move.dst);
        break;
    case STMT_IF:
        ast_expr_free(stmt->if_stmt.cond);
        for (size_t i = 0; i < stmt->if_stmt.then_count; i++) {
            ast_stmt_free(stmt->if_stmt.then_stmts[i]);
        }
        free(stmt->if_stmt.then_stmts);
        for (size_t i = 0; i < stmt->if_stmt.else_count; i++) {
            ast_stmt_free(stmt->if_stmt.else_stmts[i]);
        }
        free(stmt->if_stmt.else_stmts);
        break;
    case STMT_RETURN:
        if (stmt->return_stmt.expr) {
            ast_expr_free(stmt->return_stmt.expr);
        }
        break;
    case STMT_EXPR:
        ast_expr_free(stmt->expr_stmt);
        break;
    }
    free(stmt);
}

/* --- Parameters --- */
AstParam *ast_param_new(const char *name, size_t len, AstType *type) {
    AstParam *p = xcalloc(1, sizeof(*p));
    p->name = xstrndup(name, len);
    p->type = type;
    return p;
}

void ast_param_free(AstParam *param) {
    if (!param) {
        return;
    }
    free(param->name);
    if (param->type) {
        ast_type_free(param->type);
    }
    free(param);
}

/* --- Documentation submodules --- */
AstDocBlock *ast_doc_block_new(const char *target_name, size_t len, Loc loc) {
    AstDocBlock *d = xcalloc(1, sizeof(*d));
    d->target_name = xstrndup(target_name, len);
    d->loc = loc;
    return d;
}

void ast_doc_block_add_arg(AstDocBlock *doc, const char *name, const char *type, const char *desc) {
    assert(doc);
    da_grow(doc->args, doc->arg_count, doc->arg_capacity);
    AstDocArg *a = xcalloc(1, sizeof(*a));
    a->name = xstrdup(name ? name : "");
    a->type = xstrdup(type ? type : "");
    a->desc = xstrdup(desc ? desc : "");
    doc->args[doc->arg_count++] = a;
}

void ast_doc_block_add_return(AstDocBlock *doc, const char *name, const char *type,
                              const char *desc) {
    assert(doc);
    da_grow(doc->returns, doc->return_count, doc->return_capacity);
    AstDocReturn *r = xcalloc(1, sizeof(*r));
    r->name = xstrdup(name ? name : "");
    r->type = xstrdup(type ? type : "");
    r->desc = xstrdup(desc ? desc : "");
    doc->returns[doc->return_count++] = r;
}

void ast_doc_block_set_description(AstDocBlock *doc, const char *desc) {
    assert(doc);
    free(doc->description);
    doc->description = desc ? xstrdup(desc) : NULL;
}

void ast_doc_block_add_field(AstDocBlock *doc, const char *key, const char *value) {
    assert(doc);
    da_grow(doc->extra_fields, doc->extra_field_count, doc->extra_field_capacity);
    AstDocField *f = xcalloc(1, sizeof(*f));
    f->key = xstrdup(key ? key : "");
    f->value = xstrdup(value ? value : "");
    doc->extra_fields[doc->extra_field_count++] = f;
}

void ast_doc_block_free(AstDocBlock *doc) {
    if (!doc) {
        return;
    }
    free(doc->target_name);
    for (size_t i = 0; i < doc->arg_count; i++) {
        free(doc->args[i]->name);
        free(doc->args[i]->type);
        free(doc->args[i]->desc);
        free(doc->args[i]);
    }
    free(doc->args);
    for (size_t i = 0; i < doc->return_count; i++) {
        free(doc->returns[i]->name);
        free(doc->returns[i]->type);
        free(doc->returns[i]->desc);
        free(doc->returns[i]);
    }
    free(doc->returns);
    free(doc->description);
    for (size_t i = 0; i < doc->extra_field_count; i++) {
        free(doc->extra_fields[i]->key);
        free(doc->extra_fields[i]->value);
        free(doc->extra_fields[i]);
    }
    free(doc->extra_fields);
    free(doc);
}

/* --- Blocks and program --- */
AstBlock *ast_block_new(const char *name, size_t len) {
    AstBlock *b = xcalloc(1, sizeof(*b));
    b->name = xstrndup(name, len);
    return b;
}

void ast_block_add_param(AstBlock *block, const char *name, size_t len, AstType *type) {
    assert(block);
    da_grow(block->params, block->param_count, block->param_capacity);
    block->params[block->param_count++] = ast_param_new(name, len, type);
}

void ast_block_add_stmt(AstBlock *block, AstStmt *stmt) {
    assert(block && stmt);
    da_grow(block->stmts, block->stmt_count, block->stmt_capacity);
    block->stmts[block->stmt_count++] = stmt;
}

void ast_block_free(AstBlock *block) {
    if (!block) {
        return;
    }
    free(block->name);
    free(block->module_name);
    if (block->ret_type) {
        ast_type_free(block->ret_type);
    }
    for (size_t i = 0; i < block->param_count; i++) {
        ast_param_free(block->params[i]);
    }
    free(block->params);
    for (size_t i = 0; i < block->stmt_count; i++) {
        ast_stmt_free(block->stmts[i]);
    }
    free(block->stmts);
    free(block);
}

AstProgram *ast_program_new(const char *module_name) {
    AstProgram *p = xcalloc(1, sizeof(*p));
    p->module_name = xstrdup(module_name ? module_name : "main");
    return p;
}

void ast_program_add_block(AstProgram *program, AstBlock *block) {
    assert(program && block);
    /* Keep the declaring module when an imported block changes programs.  */
    if (!block->module_name) {
        block->module_name = xstrdup(program->module_name);
    }
    da_grow(program->blocks, program->block_count, program->block_capacity);
    program->blocks[program->block_count++] = block;
}

void ast_program_add_doc_block(AstProgram *program, AstDocBlock *doc) {
    assert(program && doc);
    da_grow(program->doc_blocks, program->doc_block_count, program->doc_block_capacity);
    program->doc_blocks[program->doc_block_count++] = doc;
}

void ast_program_add_link_lib(AstProgram *program, const char *lib) {
    if (!program || !lib || !*lib) {
        return;
    }
    for (size_t i = 0; i < program->link_lib_count; i++) {
        if (strcmp(program->link_libs[i], lib) == 0) {
            return;
        }
    }
    da_grow(program->link_libs, program->link_lib_count, program->link_lib_capacity);
    program->link_libs[program->link_lib_count++] = xstrdup(lib);
}

static void check_stmt_for_link(AstProgram *program, AstStmt *stmt) {
    if (!stmt) {
        return;
    }
    if (stmt->kind == STMT_EXPR && stmt->expr_stmt && stmt->expr_stmt->kind == EXPR_CALL) {
        AstExpr *callee = stmt->expr_stmt->call.callee;
        if (callee && callee->kind == EXPR_MEMBER && strcmp(callee->member.member, "link") == 0) {
            if (callee->member.target && callee->member.target->kind == EXPR_IDENTIFIER &&
                strcmp(callee->member.target->ident_name, "mod") == 0) {
                for (size_t i = 0; i < stmt->expr_stmt->call.arg_count; i++) {
                    AstExpr *arg = stmt->expr_stmt->call.args[i];
                    if (arg->kind == EXPR_STRING_LIT && arg->str_val) {
                        ast_program_add_link_lib(program, arg->str_val);
                    } else if (arg->kind == EXPR_IDENTIFIER && arg->ident_name) {
                        ast_program_add_link_lib(program, arg->ident_name);
                    }
                }
            }
        }
    }
}

void ast_program_collect_link_libs(AstProgram *program) {
    if (!program) {
        return;
    }
    for (size_t b = 0; b < program->block_count; b++) {
        AstBlock *block = program->blocks[b];
        for (size_t s = 0; s < block->stmt_count; s++) {
            check_stmt_for_link(program, block->stmts[s]);
        }
    }
}

void ast_program_free(AstProgram *program) {
    if (!program) {
        return;
    }
    free(program->module_name);
    for (size_t i = 0; i < program->block_count; i++) {
        ast_block_free(program->blocks[i]);
    }
    free(program->blocks);
    for (size_t i = 0; i < program->doc_block_count; i++) {
        ast_doc_block_free(program->doc_blocks[i]);
    }
    free(program->doc_blocks);
    for (size_t i = 0; i < program->extern_block_count; i++) {
        ast_extern_block_free(program->extern_blocks[i]);
    }
    free(program->extern_blocks);
    for (size_t i = 0; i < program->link_lib_count; i++) {
        free(program->link_libs[i]);
    }
    free(program->link_libs);
    free(program);
}

/* --- Foreign submodules --- */
AstExternBlock *ast_extern_block_new(const char *name, size_t len, Loc loc) {
    AstExternBlock *eb = xcalloc(1, sizeof(*eb));
    eb->name = xstrndup(name, len);
    eb->language = xstrdup("c");
    eb->loc = loc;
    return eb;
}

void ast_extern_block_add_param(AstExternBlock *block, const char *name, size_t len,
                                AstType *type) {
    assert(block);
    da_grow(block->params, block->param_count, block->param_capacity);
    block->params[block->param_count++] = ast_param_new(name, len, type);
}

void ast_extern_block_set_code(AstExternBlock *block, const char *code) {
    assert(block);
    free(block->raw_code);
    block->raw_code = code ? xstrdup(code) : NULL;
}

void ast_extern_block_free(AstExternBlock *block) {
    if (!block) {
        return;
    }
    free(block->name);
    free(block->module_name);
    free(block->language);
    free(block->raw_code);
    if (block->ret_type) {
        ast_type_free(block->ret_type);
    }
    for (size_t i = 0; i < block->param_count; i++) {
        ast_param_free(block->params[i]);
    }
    free(block->params);
    free(block);
}

void ast_program_add_extern_block(AstProgram *program, AstExternBlock *block) {
    assert(program && block);
    if (!block->module_name) {
        block->module_name = xstrdup(program->module_name);
    }
    da_grow(program->extern_blocks, program->extern_block_count, program->extern_block_capacity);
    program->extern_blocks[program->extern_block_count++] = block;
}

AstType *ast_type_clone(const AstType *t) {
    if (!t) {
        return NULL;
    }
    if (t->kind == TYPE_POINTER) {
        return ast_type_new_pointer(ast_type_clone(t->base_type));
    }
    if (t->kind == TYPE_CUSTOM) {
        return ast_type_new_custom(t->custom_name);
    }
    return ast_type_new(t->kind);
}

AstExpr *ast_expr_clone(const AstExpr *e) {
    if (!e) {
        return NULL;
    }
    switch (e->kind) {
    case EXPR_INT_LIT:
        return ast_expr_new_int(e->int_val, e->loc);
    case EXPR_FLOAT_LIT:
        return ast_expr_new_float(e->float_val, e->loc);
    case EXPR_STRING_LIT:
        return ast_expr_new_string(e->str_val, strlen(e->str_val), e->loc);
    case EXPR_BOOL_LIT:
        return ast_expr_new_bool(e->bool_val, e->loc);
    case EXPR_NULL_LIT:
        return ast_expr_new_null(e->loc);
    case EXPR_IDENTIFIER:
        return ast_expr_new_ident(e->ident_name, strlen(e->ident_name), e->loc);
    case EXPR_BINARY:
        return ast_expr_new_binary(e->binary.op, ast_expr_clone(e->binary.left),
                                   ast_expr_clone(e->binary.right), e->loc);
    case EXPR_UNARY:
        return ast_expr_new_unary(e->unary.op, ast_expr_clone(e->unary.operand), e->loc);
    case EXPR_CALL: {
        AstExpr **args = NULL;
        if (e->call.arg_count > 0) {
            args = xmalloc(e->call.arg_count * sizeof(*args));
            for (size_t i = 0; i < e->call.arg_count; i++) {
                args[i] = ast_expr_clone(e->call.args[i]);
            }
        }
        return ast_expr_new_call(ast_expr_clone(e->call.callee), args, e->call.arg_count, e->loc);
    }
    case EXPR_MEMBER:
        return ast_expr_new_member(ast_expr_clone(e->member.target), e->member.member,
                                   strlen(e->member.member), e->loc);
    case EXPR_PATH:
        return ast_expr_new_path(ast_expr_clone(e->path.target), e->path.subpath,
                                 strlen(e->path.subpath), e->loc);
    }
    return NULL;
}

/* --- AST dump --- */
static void print_indent(int depth, bool is_last) {
    for (int i = 0; i < depth - 1; i++) {
        printf("│   ");
    }
    if (depth > 0) {
        printf("%s ", is_last ? "└──" : "├──");
    }
}

static void print_expr(const AstExpr *e, int depth, bool is_last);

static void print_type_annot(const AstType *t) {
    if (!t) {
        return;
    }
    if (t->kind == TYPE_POINTER) {
        printf("*");
        print_type_annot(t->base_type);
    } else {
        printf("%s", ast_type_to_string(t));
    }
}

static void print_expr(const AstExpr *e, int depth, bool is_last) {
    if (!e) {
        return;
    }
    print_indent(depth, is_last);
    switch (e->kind) {
    case EXPR_INT_LIT:
        printf("Int: %ld\n", (long)e->int_val);
        break;
    case EXPR_FLOAT_LIT:
        printf("Float: %f\n", e->float_val);
        break;
    case EXPR_STRING_LIT:
        printf("String: \"%s\"\n", e->str_val);
        break;
    case EXPR_BOOL_LIT:
        printf("Bool: %s\n", e->bool_val ? "true" : "false");
        break;
    case EXPR_NULL_LIT:
        printf("Null\n");
        break;
    case EXPR_IDENTIFIER:
        printf("Ident: %s\n", e->ident_name);
        break;
    case EXPR_BINARY:
        printf("BinaryOp (%s)\n", token_type_name(e->binary.op));
        print_expr(e->binary.left, depth + 1, false);
        print_expr(e->binary.right, depth + 1, true);
        break;
    case EXPR_UNARY:
        printf("UnaryOp (%s)\n", token_type_name(e->unary.op));
        print_expr(e->unary.operand, depth + 1, true);
        break;
    case EXPR_CALL:
        printf("Call\n");
        print_indent(depth + 1, false);
        printf("Callee:\n");
        print_expr(e->call.callee, depth + 2, true);
        for (size_t i = 0; i < e->call.arg_count; i++) {
            print_indent(depth + 1, i == e->call.arg_count - 1);
            printf("Arg %zu:\n", i);
            print_expr(e->call.args[i], depth + 2, true);
        }
        break;
    case EXPR_MEMBER:
        printf("MemberAccess (.%s)\n", e->member.member);
        print_expr(e->member.target, depth + 1, true);
        break;
    case EXPR_PATH:
        printf("Path (:%s)\n", e->path.subpath);
        print_expr(e->path.target, depth + 1, true);
        break;
    }
}

static void print_stmt(const AstStmt *s, int depth, bool is_last) {
    if (!s) {
        return;
    }
    print_indent(depth, is_last);
    switch (s->kind) {
    case STMT_VAR_DECL:
        printf("VarDecl '%s' : ", s->var_decl.name);
        print_type_annot(s->var_decl.type);
        printf("\n");
        if (s->var_decl.init_expr) {
            print_expr(s->var_decl.init_expr, depth + 1, true);
        }
        break;
    case STMT_ASSIGN:
        printf("Assign\n");
        print_expr(s->assign.target, depth + 1, false);
        print_expr(s->assign.value, depth + 1, true);
        break;
    case STMT_MOVE:
        printf("Move >> %s\n", s->move.dst);
        if (s->move.src_expr) {
            print_expr(s->move.src_expr, depth + 1, true);
        }
        break;
    case STMT_IF:
        printf("If\n");
        print_indent(depth + 1, false);
        printf("Condition:\n");
        print_expr(s->if_stmt.cond, depth + 2, true);
        print_indent(depth + 1, s->if_stmt.else_count == 0);
        printf("Then Body (%zu):\n", s->if_stmt.then_count);
        for (size_t i = 0; i < s->if_stmt.then_count; i++) {
            print_stmt(s->if_stmt.then_stmts[i], depth + 2, i == s->if_stmt.then_count - 1);
        }
        if (s->if_stmt.else_count > 0) {
            print_indent(depth + 1, true);
            printf("Else Body (%zu):\n", s->if_stmt.else_count);
            for (size_t i = 0; i < s->if_stmt.else_count; i++) {
                print_stmt(s->if_stmt.else_stmts[i], depth + 2, i == s->if_stmt.else_count - 1);
            }
        }
        break;
    case STMT_RETURN:
        printf("Return\n");
        if (s->return_stmt.expr) {
            print_expr(s->return_stmt.expr, depth + 1, true);
        }
        break;
    case STMT_EXPR:
        printf("ExprStmt\n");
        print_expr(s->expr_stmt, depth + 1, true);
        break;
    }
}

void ast_print(const AstProgram *prog) {
    if (!prog) {
        printf("(empty program)\n");
        return;
    }
    printf("Program (Module: %s)\n", prog->module_name);
    for (size_t i = 0; i < prog->block_count; i++) {
        AstBlock *b = prog->blocks[i];
        bool is_last_block = (i == prog->block_count - 1 && prog->doc_block_count == 0);
        print_indent(1, is_last_block);
        printf("Block -[%s]", b->name);
        if (b->param_count > 0) {
            printf(" {");
            for (size_t p = 0; p < b->param_count; p++) {
                if (p > 0) {
                    printf(", ");
                }
                printf("%s: %s", b->params[p]->name, ast_type_to_string(b->params[p]->type));
            }
            printf("}");
        }
        if (b->ret_type) {
            printf(" -> %s", ast_type_to_string(b->ret_type));
        }
        printf(" (%zu stmts)\n", b->stmt_count);
        for (size_t j = 0; j < b->stmt_count; j++) {
            print_stmt(b->stmts[j], 2, j == b->stmt_count - 1);
        }
    }

    for (size_t i = 0; i < prog->doc_block_count; i++) {
        AstDocBlock *d = prog->doc_blocks[i];
        bool is_last_doc = (i == prog->doc_block_count - 1);
        print_indent(1, is_last_doc);
        printf("DocBlock #[%s]\n", d->target_name);
        if (d->arg_count > 0) {
            print_indent(2, false);
            printf("Arguments (%zu):\n", d->arg_count);
            for (size_t a = 0; a < d->arg_count; a++) {
                print_indent(3, a == d->arg_count - 1);
                printf("%s: %s - %s\n", d->args[a]->name, d->args[a]->type, d->args[a]->desc);
            }
        }
        if (d->return_count > 0) {
            print_indent(2, false);
            printf("Returns (%zu):\n", d->return_count);
            for (size_t r = 0; r < d->return_count; r++) {
                print_indent(3, r == d->return_count - 1);
                printf("%s: %s - %s\n", d->returns[r]->name, d->returns[r]->type,
                       d->returns[r]->desc);
            }
        }
        if (d->description) {
            print_indent(2, true);
            printf("Description: %s\n", d->description);
        }
    }

    for (size_t i = 0; i < prog->extern_block_count; i++) {
        AstExternBlock *eb = prog->extern_blocks[i];
        bool is_last_ext = (i == prog->extern_block_count - 1);
        print_indent(1, is_last_ext);
        printf("ExternBlock ![%s] (lang: %s)", eb->name, eb->language ? eb->language : "c");
        if (eb->param_count > 0) {
            printf(" {");
            for (size_t p = 0; p < eb->param_count; p++) {
                if (p > 0) {
                    printf(", ");
                }
                printf("%s: %s", eb->params[p]->name, ast_type_to_string(eb->params[p]->type));
            }
            printf("}");
        }
        if (eb->ret_type) {
            printf(" -> %s", ast_type_to_string(eb->ret_type));
        }
        printf(" [%zu bytes of foreign code]\n", eb->raw_code ? strlen(eb->raw_code) : 0);
    }
}
