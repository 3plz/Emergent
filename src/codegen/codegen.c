/* Intermediate code generator from Emergent AST to QBE IL.
   Copyright (C) 2026 Emergent Compiler Authors.  */

#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "../common.h"
#include "../utils/strbuf.h"
#include "func_table.h"
#include "pool.h"
#include "scope.h"
#include "types.h"
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    StrBuf data_sb;
    StrBuf alloc_sb;
    StrBuf text_sb;
    StringPool pool;
    Scope scope;
    FuncTable func_table;
    const char *module_name;
    TypeInfo ret_ti;
    Loc loc;
    int temp_counter;
    int label_counter;
    bool block_terminated;
    CodegenResult *result;
} CodegenCtx;

static void set_error(CodegenCtx *ctx, Loc loc, const char *fmt, ...) {
    if (ctx->result->had_error) {
        return;
    }
    ctx->result->had_error = true;
    ctx->result->error_line = loc.line;
    ctx->result->error_column = loc.col;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->result->error_msg, sizeof(ctx->result->error_msg), fmt, args);
    va_end(args);
}

static void new_temp(CodegenCtx *ctx, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%%t%d", ctx->temp_counter++);
}

static int new_label_id(CodegenCtx *ctx) {
    return ctx->label_counter++;
}

static GenVal emit_to_bool(CodegenCtx *ctx, GenVal val) {
    if (ctx->result->had_error) {
        return val;
    }
    if (val.ti.kind == TYPE_VOID) {
        set_error(ctx, ctx->loc, "A void expression cannot be used as a value");
        return val;
    }

    if (val.is_const) {
        GenVal out;
        memset(&out, 0, sizeof(out));
        out.ti = get_type_info_kind(TYPE_BOOL);
        out.is_const = true;

        if (val.ti.is_float) {
            double d = 0.0;
            if (strncmp(val.repr, "d_", 2) == 0 || strncmp(val.repr, "s_", 2) == 0) {
                d = strtod(val.repr + 2, NULL);
            } else {
                d = strtod(val.repr, NULL);
            }
            snprintf(out.repr, sizeof(out.repr), "%d", (d != 0.0) ? 1 : 0);
            return out;
        } else if (val.repr[0] == '$') {
            snprintf(out.repr, sizeof(out.repr), "1");
            return out;
        } else {
            long long v = strtoll(val.repr, NULL, 0);
            snprintf(out.repr, sizeof(out.repr), "%d", (v != 0) ? 1 : 0);
            return out;
        }
    }

    char tmp[64];
    new_temp(ctx, tmp, sizeof(tmp));

    if (val.ti.is_float) {
        if (val.ti.cls == QBE_CLS_D) {
            sb_printf(&ctx->text_sb, "    %s =w cned %s, d_0.0\n", tmp, val.repr);
        } else {
            sb_printf(&ctx->text_sb, "    %s =w cnes %s, s_0.0\n", tmp, val.repr);
        }
    } else if (val.ti.cls == QBE_CLS_L) {
        sb_printf(&ctx->text_sb, "    %s =w cnel %s, 0\n", tmp, val.repr);
    } else {
        sb_printf(&ctx->text_sb, "    %s =w cnew %s, 0\n", tmp, val.repr);
    }

    GenVal out;
    memset(&out, 0, sizeof(out));
    snprintf(out.repr, sizeof(out.repr), "%s", tmp);
    out.ti = get_type_info_kind(TYPE_BOOL);
    out.is_const = false;
    return out;
}

static bool check_int_literal_range(CodegenCtx *ctx, const char *repr, TypeInfo target) {
    if (!is_int_literal(repr)) {
        return true;
    }
    char *endptr = NULL;
    long long n = strtoll(repr, &endptr, 0);

    bool out_of_range = false;
    const char *type_name = "int";

    switch (target.kind) {
    case TYPE_I8:
        out_of_range = (n < -128 || n > 127);
        type_name = "i8";
        break;
    case TYPE_U8:
        out_of_range = (n < 0 || n > 255);
        type_name = "u8";
        break;
    case TYPE_I16:
        out_of_range = (n < -32768 || n > 32767);
        type_name = "i16";
        break;
    case TYPE_U16:
        out_of_range = (n < 0 || n > 65535);
        type_name = "u16";
        break;
    case TYPE_I32:
    case TYPE_INT:
    case TYPE_CUSTOM:
        out_of_range = (n < -2147483648LL || n > 2147483647LL);
        type_name = "int / i32";
        break;
    case TYPE_U32:
    case TYPE_UINT:
        out_of_range = (n < 0 || (unsigned long long)n > 4294967295ULL);
        type_name = "uint / u32";
        break;
    default:
        break;
    }

    if (out_of_range) {
        set_error(ctx, ctx->loc, "Integer literal %s exceeds allowed range for type '%s'", repr,
                  type_name);
        return false;
    }
    return true;
}

static GenVal coerce(CodegenCtx *ctx, GenVal val, TypeInfo target) {
    if (ctx->result->had_error) {
        return val;
    }
    if (val.ti.kind == TYPE_VOID || target.kind == TYPE_VOID) {
        set_error(ctx, ctx->loc, "A void expression cannot be used as a value");
        return val;
    }

    if (target.kind == TYPE_BOOL) {
        return emit_to_bool(ctx, val);
    }

    if (val.is_const && is_int_literal(val.repr) && !target.is_float) {
        if (!check_int_literal_range(ctx, val.repr, target)) {
            return val;
        }
    }

    if (val.ti.cls == target.cls && val.ti.is_float == target.is_float) {
        val.ti = target;
        return val;
    }

    if (val.is_const) {
        if (target.is_float && is_int_literal(val.repr)) {
            GenVal out;
            memset(&out, 0, sizeof(out));
            const char *prefix = (target.cls == QBE_CLS_D) ? "d" : "s";
            snprintf(out.repr, sizeof(out.repr), "%s_%.240s.0", prefix, val.repr);
            out.ti = target;
            out.is_const = true;
            return out;
        }

        if (target.cls == QBE_CLS_S && strncmp(val.repr, "d_", 2) == 0) {
            GenVal out;
            memset(&out, 0, sizeof(out));
            snprintf(out.repr, sizeof(out.repr), "s_%s", val.repr + 2);
            out.ti = target;
            out.is_const = true;
            return out;
        }

        if (target.cls == QBE_CLS_D && strncmp(val.repr, "s_", 2) == 0) {
            GenVal out;
            memset(&out, 0, sizeof(out));
            snprintf(out.repr, sizeof(out.repr), "d_%s", val.repr + 2);
            out.ti = target;
            out.is_const = true;
            return out;
        }

        if (target.cls == QBE_CLS_L && !target.is_float && is_int_literal(val.repr)) {
            val.ti = target;
            return val;
        }
    }

    char tmp[64];
    new_temp(ctx, tmp, sizeof(tmp));

    if (!val.ti.is_float && target.is_float) {
        const char *t_str = cls_str(target.cls);
        const char *conv;
        if (val.ti.cls == QBE_CLS_L) {
            conv = val.ti.is_signed ? "sltof" : "ultof";
        } else {
            conv = val.ti.is_signed ? "swtof" : "uwtof";
        }

        sb_printf(&ctx->text_sb, "    %s =%s %s %s\n", tmp, t_str, conv, val.repr);
        GenVal out;
        memset(&out, 0, sizeof(out));
        snprintf(out.repr, sizeof(out.repr), "%s", tmp);
        out.ti = target;
        out.is_const = false;
        return out;
    }

    if (val.ti.is_float && target.is_float) {
        if (val.ti.cls == QBE_CLS_S && target.cls == QBE_CLS_D) {
            sb_printf(&ctx->text_sb, "    %s =d exts %s\n", tmp, val.repr);
        } else if (val.ti.cls == QBE_CLS_D && target.cls == QBE_CLS_S) {
            sb_printf(&ctx->text_sb, "    %s =s truncd %s\n", tmp, val.repr);
        }

        GenVal out;
        memset(&out, 0, sizeof(out));
        snprintf(out.repr, sizeof(out.repr), "%s", tmp);
        out.ti = target;
        out.is_const = false;
        return out;
    }

    if (val.ti.is_float && !target.is_float) {
        /* For unsigned u32, convert to signed 64-bit long first, then copy low 32 bits.  */
        if (!target.is_signed && target.cls == QBE_CLS_W) {
            char l_tmp[64];
            new_temp(ctx, l_tmp, sizeof(l_tmp));
            if (val.ti.cls == QBE_CLS_S) {
                char d_prom[64];
                new_temp(ctx, d_prom, sizeof(d_prom));
                sb_printf(&ctx->text_sb, "    %s =d exts %s\n", d_prom, val.repr);
                sb_printf(&ctx->text_sb, "    %s =l dtosi %s\n", l_tmp, d_prom);
            } else {
                sb_printf(&ctx->text_sb, "    %s =l dtosi %s\n", l_tmp, val.repr);
            }
            sb_printf(&ctx->text_sb, "    %s =w copy %s\n", tmp, l_tmp);
        } else {
            const char *t_str = cls_str(target.cls);
            const char *conv = (val.ti.cls == QBE_CLS_D) ? "dtosi" : "stosi";
            sb_printf(&ctx->text_sb, "    %s =%s %s %s\n", tmp, t_str, conv, val.repr);
        }

        GenVal out;
        memset(&out, 0, sizeof(out));
        snprintf(out.repr, sizeof(out.repr), "%s", tmp);
        out.ti = target;
        out.is_const = false;
        return out;
    }

    if (val.ti.cls == QBE_CLS_W && target.cls == QBE_CLS_L) {
        const char *ext = val.ti.is_signed ? "extsw" : "extuw";
        sb_printf(&ctx->text_sb, "    %s =l %s %s\n", tmp, ext, val.repr);
        GenVal out;
        memset(&out, 0, sizeof(out));
        snprintf(out.repr, sizeof(out.repr), "%s", tmp);
        out.ti = target;
        out.is_const = false;
        return out;
    }

    if (val.ti.cls == QBE_CLS_L && target.cls == QBE_CLS_W) {
        sb_printf(&ctx->text_sb, "    %s =w copy %s\n", tmp, val.repr);
        GenVal out;
        memset(&out, 0, sizeof(out));
        snprintf(out.repr, sizeof(out.repr), "%s", tmp);
        out.ti = target;
        out.is_const = false;
        return out;
    }

    val.ti = target;
    return val;
}

static void emit_load(CodegenCtx *ctx, const char *dest_tmp, const char *addr, TypeInfo ti) {
    if (ti.is_float) {
        if (ti.cls == QBE_CLS_D) {
            sb_printf(&ctx->text_sb, "    %s =d loadd %s\n", dest_tmp, addr);
        } else {
            sb_printf(&ctx->text_sb, "    %s =s loads %s\n", dest_tmp, addr);
        }
        return;
    }

    if (ti.size_bytes == 1) {
        sb_printf(&ctx->text_sb, "    %s =w %s %s\n", dest_tmp, ti.is_signed ? "loadsb" : "loadub",
                  addr);
    } else if (ti.size_bytes == 2) {
        sb_printf(&ctx->text_sb, "    %s =w %s %s\n", dest_tmp, ti.is_signed ? "loadsh" : "loaduh",
                  addr);
    } else if (ti.size_bytes == 4) {
        sb_printf(&ctx->text_sb, "    %s =w loadw %s\n", dest_tmp, addr);
    } else {
        sb_printf(&ctx->text_sb, "    %s =l loadl %s\n", dest_tmp, addr);
    }
}

static void emit_store(CodegenCtx *ctx, GenVal val, const char *addr, TypeInfo dest_ti) {
    val = coerce(ctx, val, dest_ti);
    if (ctx->result->had_error) {
        return;
    }

    if (dest_ti.is_float) {
        if (dest_ti.cls == QBE_CLS_D) {
            sb_printf(&ctx->text_sb, "    stored %s, %s\n", val.repr, addr);
        } else {
            sb_printf(&ctx->text_sb, "    stores %s, %s\n", val.repr, addr);
        }
        return;
    }

    if (dest_ti.size_bytes == 1) {
        sb_printf(&ctx->text_sb, "    storeb %s, %s\n", val.repr, addr);
    } else if (dest_ti.size_bytes == 2) {
        sb_printf(&ctx->text_sb, "    storeh %s, %s\n", val.repr, addr);
    } else if (dest_ti.size_bytes == 4) {
        sb_printf(&ctx->text_sb, "    storew %s, %s\n", val.repr, addr);
    } else {
        sb_printf(&ctx->text_sb, "    storel %s, %s\n", val.repr, addr);
    }
}

static bool call_target(const AstExpr *callee, const char **module_name, const char **name) {
    *module_name = NULL;
    if (callee->kind == EXPR_IDENTIFIER) {
        *name = callee->ident_name;
        return true;
    }
    if (callee->kind == EXPR_MEMBER && callee->member.target->kind == EXPR_IDENTIFIER) {
        *module_name = callee->member.target->ident_name;
        *name = callee->member.member;
        return true;
    }
    return false;
}

static bool is_module_directive(const AstExpr *callee) {
    const char *module_name, *name;
    if (!call_target(callee, &module_name, &name) || !module_name) {
        return false;
    }
    return (strcmp(module_name, "mod") == 0 &&
            (strcmp(name, "add") == 0 || strcmp(name, "link") == 0)) ||
           (strcmp(module_name, "sub") == 0 && strcmp(name, "add") == 0);
}

static bool is_syscall_target(const AstExpr *callee) {
    const char *module_name, *name;
    if (!call_target(callee, &module_name, &name)) {
        return false;
    }
    if (!module_name) {
        return strcasecmp(name, "Syscall") == 0;
    }
    return strcmp(module_name, "sys") == 0 &&
           (strcasecmp(name, "Call") == 0 || strcasecmp(name, "Syscall") == 0);
}

static GenVal gen_expr(CodegenCtx *ctx, const AstExpr *e);

static GenVal gen_call(CodegenCtx *ctx, const AstExpr *e, bool foreign_call) {
    assert(e->kind == EXPR_CALL);
    GenVal v = {0};
    if (ctx->result->had_error) {
        return v;
    }
    ctx->loc = e->loc;

    /* Module directives mod.add(...), sub.add(...), mod.link(...) have no runtime effect.  */
    if (!foreign_call && is_module_directive(e->call.callee)) {
        v.ti = get_type_info_kind(TYPE_VOID);
        return v;
    }

    if (!foreign_call && e->call.callee->kind == EXPR_IDENTIFIER &&
        strcmp(e->call.callee->ident_name, "ExternC") == 0) {
        if (e->call.arg_count != 1 || e->call.args[0]->kind != EXPR_CALL) {
            set_error(ctx, e->loc, "ExternC requires exactly one foreign C function call");
            return v;
        }
        return gen_call(ctx, e->call.args[0], true);
    }

    GenVal *eval_args = NULL;
    if (e->call.arg_count > 0) {
        eval_args = xmalloc(e->call.arg_count * sizeof(*eval_args));
        for (size_t i = 0; i < e->call.arg_count; i++) {
            eval_args[i] = gen_expr(ctx, e->call.args[i]);
            if (ctx->result->had_error) {
                free(eval_args);
                return v;
            }
        }
    }

    if (!foreign_call && is_syscall_target(e->call.callee)) {
        if (e->call.arg_count == 0) {
            set_error(ctx, e->loc, "Syscall requires at least a system call number");
            if (eval_args) {
                free(eval_args);
            }
            return v;
        }

        char tmp[64];
        new_temp(ctx, tmp, sizeof(tmp));
        sb_printf(&ctx->text_sb, "    %s =l call $syscall(", tmp);
        for (size_t i = 0; i < e->call.arg_count; i++) {
            if (eval_args[i].ti.kind == TYPE_VOID) {
                set_error(ctx, e->loc, "A void expression cannot be a syscall argument");
                free(eval_args);
                return v;
            }
            if (i > 0) {
                sb_printf(&ctx->text_sb, ", ");
            }
            sb_printf(&ctx->text_sb, "%s %s", cls_str(eval_args[i].ti.cls), eval_args[i].repr);
        }
        sb_printf(&ctx->text_sb, ")\n");
        if (eval_args) {
            free(eval_args);
        }

        snprintf(v.repr, sizeof(v.repr), "%s", tmp);
        v.ti = get_type_info_kind(TYPE_I64);
        v.is_const = false;
        return v;
    }

    const char *module_name, *callee_name;
    if (!call_target(e->call.callee, &module_name, &callee_name)) {
        free(eval_args);
        set_error(ctx, e->loc, "Unsupported function call");
        return v;
    }

    TypeInfo *arg_types =
        e->call.arg_count ? xmalloc(e->call.arg_count * sizeof(*arg_types)) : NULL;
    for (size_t i = 0; i < e->call.arg_count; i++) {
        arg_types[i] = eval_args[i].ti;
    }
    FuncSignature *fn;
    FuncResolution resolution = func_table_resolve(&ctx->func_table, module_name, ctx->module_name,
                                                   callee_name, arg_types, e->call.arg_count, &fn);
    free(arg_types);
    if (resolution != FUNC_FOUND) {
        const char *reason = resolution == FUNC_AMBIGUOUS  ? "Ambiguous overload"
                             : resolution == FUNC_NO_MATCH ? "No matching overload"
                                                           : "Unknown function";
        set_error(ctx, e->loc, "%s for '%s%s%s' with %zu argument(s)", reason,
                  module_name ? module_name : "", module_name ? "." : "", callee_name,
                  e->call.arg_count);
        free(eval_args);
        return v;
    }

    /* ExternC authorizes this call alone, not calls nested in its arguments.  */
    if (fn->is_external != foreign_call) {
        if (fn->is_external) {
            set_error(ctx, e->loc, "C function '%s%s%s' must be called through ExternC",
                      module_name ? module_name : "", module_name ? "." : "", callee_name);
        } else {
            set_error(ctx, e->loc,
                      "ExternC requires a foreign C function, but '%s' is an Emergent function",
                      callee_name);
        }
        free(eval_args);
        return v;
    }

    for (size_t i = 0; i < e->call.arg_count; i++) {
        ctx->loc = e->call.args[i]->loc;
        eval_args[i] = coerce(ctx, eval_args[i], get_type_info(fn->params[i]->type));
    }
    if (ctx->result->had_error) {
        free(eval_args);
        return v;
    }

    v.ti = fn->ret_ti;
    if (v.ti.kind == TYPE_VOID) {
        sb_printf(&ctx->text_sb, "    call $%s(", fn->mangled_name);
    } else {
        new_temp(ctx, v.repr, sizeof(v.repr));
        sb_printf(&ctx->text_sb, "    %s =%s call $%s(", v.repr, cls_str(v.ti.cls),
                  fn->mangled_name);
    }
    for (size_t i = 0; i < e->call.arg_count; i++) {
        if (i > 0) {
            sb_printf(&ctx->text_sb, ", ");
        }
        sb_printf(&ctx->text_sb, "%s %s", cls_str(eval_args[i].ti.cls), eval_args[i].repr);
    }
    sb_printf(&ctx->text_sb, ")\n");
    free(eval_args);
    return v;
}

static GenVal gen_expr(CodegenCtx *ctx, const AstExpr *e) {
    assert(ctx && e);
    GenVal v;
    memset(&v, 0, sizeof(v));
    if (ctx->result->had_error) {
        return v;
    }

    ctx->loc = e->loc;

    switch (e->kind) {
    case EXPR_INT_LIT:
        snprintf(v.repr, sizeof(v.repr), "%ld", (long)e->int_val);
        if (e->int_val > 2147483647L || e->int_val < -2147483648L) {
            v.ti = get_type_info_kind(TYPE_I64);
        } else {
            v.ti = get_type_info_kind(TYPE_INT);
        }
        v.is_const = true;
        return v;

    case EXPR_FLOAT_LIT:
        snprintf(v.repr, sizeof(v.repr), "d_%.17g", e->float_val);
        v.ti = get_type_info_kind(TYPE_F64);
        v.is_const = true;
        return v;

    case EXPR_STRING_LIT: {
        const char *label = pool_get_string(&ctx->pool, e->str_val ? e->str_val : "");
        snprintf(v.repr, sizeof(v.repr), "$%s", label);
        v.ti = get_type_info_kind(TYPE_STR);
        v.is_const = true;
        return v;
    }

    case EXPR_BOOL_LIT:
        snprintf(v.repr, sizeof(v.repr), "%d", e->bool_val ? 1 : 0);
        v.ti = get_type_info_kind(TYPE_BOOL);
        v.is_const = true;
        return v;

    case EXPR_NULL_LIT:
        snprintf(v.repr, sizeof(v.repr), "0");
        v.ti = get_type_info_kind(TYPE_POINTER);
        v.ti.is_null = true;
        v.is_const = true;
        return v;

    case EXPR_IDENTIFIER: {
        Symbol *sym = scope_find(&ctx->scope, e->ident_name);
        if (!sym) {
            set_error(ctx, e->loc, "Undeclared variable '%s'", e->ident_name);
            return v;
        }
        if (sym->is_moved) {
            set_error(ctx, e->loc, "Use of variable '%s' whose value was moved on line %d",
                      e->ident_name, sym->moved_at_line);
            return v;
        }

        char tmp[64];
        new_temp(ctx, tmp, sizeof(tmp));
        emit_load(ctx, tmp, sym->slot, sym->type_info);

        snprintf(v.repr, sizeof(v.repr), "%s", tmp);
        v.ti = sym->type_info;
        v.is_const = false;
        return v;
    }

    case EXPR_UNARY: {
        if (e->unary.op == TOK_AMPERSAND) {
            if (e->unary.operand->kind == EXPR_IDENTIFIER) {
                Symbol *sym = scope_find(&ctx->scope, e->unary.operand->ident_name);
                if (!sym) {
                    set_error(ctx, e->loc, "Undeclared variable '%s'",
                              e->unary.operand->ident_name);
                    return v;
                }
                if (sym->is_moved) {
                    set_error(ctx, e->loc, "Cannot take address of moved variable '%s'",
                              e->unary.operand->ident_name);
                    return v;
                }
                snprintf(v.repr, sizeof(v.repr), "%s", sym->slot);
                v.ti = get_type_info_kind(TYPE_POINTER);
                v.ti.elem_kind = sym->type_info.kind;
                v.ti.pointee = sym->type_info.ast_type;
                v.is_const = false;
                return v;
            } else {
                set_error(ctx, e->loc, "Address-of operator '&' can only be applied to variables");
                return v;
            }
        }

        if (e->unary.op == TOK_ASTERISK) {
            GenVal ptr = gen_expr(ctx, e->unary.operand);
            char tmp[64];
            new_temp(ctx, tmp, sizeof(tmp));

            TypeInfo elem_ti = get_pointee_type(ptr.ti);
            if (!ptr.ti.is_pointer || elem_ti.kind == TYPE_VOID) {
                set_error(ctx, e->loc, "Cannot dereference an untyped or non-pointer value");
                return v;
            }

            emit_load(ctx, tmp, ptr.repr, elem_ti);
            snprintf(v.repr, sizeof(v.repr), "%s", tmp);
            v.ti = elem_ti;
            v.is_const = false;
            return v;
        }

        if (e->unary.op == TOK_NOT) {
            GenVal val = gen_expr(ctx, e->unary.operand);
            if (val.ti.kind == TYPE_VOID) {
                set_error(ctx, e->loc, "A void expression cannot be used as a value");
                return v;
            }
            char tmp[64];
            new_temp(ctx, tmp, sizeof(tmp));
            if (val.ti.is_float) {
                if (val.ti.cls == QBE_CLS_D) {
                    sb_printf(&ctx->text_sb, "    %s =w ceqd %s, d_0.0\n", tmp, val.repr);
                } else {
                    sb_printf(&ctx->text_sb, "    %s =w ceqs %s, s_0.0\n", tmp, val.repr);
                }
            } else if (val.ti.cls == QBE_CLS_L) {
                sb_printf(&ctx->text_sb, "    %s =w ceql %s, 0\n", tmp, val.repr);
            } else {
                sb_printf(&ctx->text_sb, "    %s =w ceqw %s, 0\n", tmp, val.repr);
            }
            snprintf(v.repr, sizeof(v.repr), "%s", tmp);
            v.ti = get_type_info_kind(TYPE_BOOL);
            v.is_const = false;
            return v;
        }

        if (e->unary.op == TOK_MINUS) {
            GenVal val = gen_expr(ctx, e->unary.operand);
            if (val.ti.kind == TYPE_VOID) {
                set_error(ctx, e->loc, "A void expression cannot be used as a value");
                return v;
            }
            char tmp[64];
            new_temp(ctx, tmp, sizeof(tmp));
            const char *c_str = cls_str(val.ti.cls);
            if (val.ti.is_float) {
                sb_printf(&ctx->text_sb, "    %s =%s sub %s_0.0, %s\n", tmp, c_str, c_str,
                          val.repr);
            } else {
                sb_printf(&ctx->text_sb, "    %s =%s sub 0, %s\n", tmp, c_str, val.repr);
            }
            snprintf(v.repr, sizeof(v.repr), "%s", tmp);
            v.ti = val.ti;
            v.is_const = false;
            return v;
        }

        if (e->unary.op == TOK_PLUS) {
            return gen_expr(ctx, e->unary.operand);
        }

        set_error(ctx, e->loc, "Unsupported unary operator");
        return v;
    }

    case EXPR_BINARY: {
        GenVal left = gen_expr(ctx, e->binary.left);
        GenVal right = gen_expr(ctx, e->binary.right);
        if (left.ti.kind == TYPE_VOID || right.ti.kind == TYPE_VOID) {
            set_error(ctx, e->loc, "A void expression cannot be used as a value");
            return v;
        }
        /* DBG("emit binary op %s: lhs=%s rhs=%s", token_type_name(e->binary.op),
               left.repr, right.repr);  */

        if (e->binary.op == TOK_AND || e->binary.op == TOK_OR) {
            left = emit_to_bool(ctx, left);
            right = emit_to_bool(ctx, right);

            char res_tmp[64];
            new_temp(ctx, res_tmp, sizeof(res_tmp));

            const char *op_name = (e->binary.op == TOK_AND) ? "and" : "or";
            sb_printf(&ctx->text_sb, "    %s =w %s %s, %s\n", res_tmp, op_name, left.repr,
                      right.repr);

            snprintf(v.repr, sizeof(v.repr), "%s", res_tmp);
            v.ti = get_type_info_kind(TYPE_BOOL);
            v.is_const = false;
            return v;
        }

        TypeInfo common_ti;
        if (left.ti.is_float || right.ti.is_float) {
            common_ti = (left.ti.cls == QBE_CLS_D || right.ti.cls == QBE_CLS_D)
                            ? get_type_info_kind(TYPE_F64)
                            : get_type_info_kind(TYPE_F32);
        } else if (left.ti.cls == QBE_CLS_L || right.ti.cls == QBE_CLS_L) {
            bool is_signed = left.ti.is_signed && right.ti.is_signed;
            common_ti = get_type_info_kind(is_signed ? TYPE_I64 : TYPE_U64);
        } else {
            bool is_signed = left.ti.is_signed && right.ti.is_signed;
            common_ti = get_type_info_kind(is_signed ? TYPE_I32 : TYPE_U32);
        }

        left = coerce(ctx, left, common_ti);
        right = coerce(ctx, right, common_ti);

        if (e->binary.op >= TOK_EQUAL && e->binary.op <= TOK_LESS_EQUAL) {
            char cmp_op[16];
            const char *cls_suffix = cls_str(common_ti.cls);

            if (common_ti.is_float) {
                const char *kind = (e->binary.op == TOK_EQUAL)        ? "eq"
                                   : (e->binary.op == TOK_NOT_EQUAL)  ? "ne"
                                   : (e->binary.op == TOK_LESS)       ? "lt"
                                   : (e->binary.op == TOK_LESS_EQUAL) ? "le"
                                   : (e->binary.op == TOK_GREATER)    ? "gt"
                                                                      : "ge";
                snprintf(cmp_op, sizeof(cmp_op), "c%s%s", kind, cls_suffix);
            } else if (common_ti.is_signed) {
                const char *kind = (e->binary.op == TOK_EQUAL)        ? "eq"
                                   : (e->binary.op == TOK_NOT_EQUAL)  ? "ne"
                                   : (e->binary.op == TOK_LESS)       ? "slt"
                                   : (e->binary.op == TOK_LESS_EQUAL) ? "sle"
                                   : (e->binary.op == TOK_GREATER)    ? "sgt"
                                                                      : "sge";
                snprintf(cmp_op, sizeof(cmp_op), "c%s%s", kind, cls_suffix);
            } else {
                const char *kind = (e->binary.op == TOK_EQUAL)        ? "eq"
                                   : (e->binary.op == TOK_NOT_EQUAL)  ? "ne"
                                   : (e->binary.op == TOK_LESS)       ? "ult"
                                   : (e->binary.op == TOK_LESS_EQUAL) ? "ule"
                                   : (e->binary.op == TOK_GREATER)    ? "ugt"
                                                                      : "uge";
                snprintf(cmp_op, sizeof(cmp_op), "c%s%s", kind, cls_suffix);
            }

            char tmp[64];
            new_temp(ctx, tmp, sizeof(tmp));
            sb_printf(&ctx->text_sb, "    %s =w %s %s, %s\n", tmp, cmp_op, left.repr, right.repr);
            snprintf(v.repr, sizeof(v.repr), "%s", tmp);
            v.ti = get_type_info_kind(TYPE_BOOL);
            v.is_const = false;
            return v;
        }

        const char *instr = NULL;
        switch (e->binary.op) {
        case TOK_PLUS:
            instr = "add";
            break;
        case TOK_MINUS:
            instr = "sub";
            break;
        case TOK_ASTERISK:
            instr = "mul";
            break;
        case TOK_SLASH:
            instr = (common_ti.is_float || common_ti.is_signed) ? "div" : "udiv";
            break;
        case TOK_PERCENT:
            instr = common_ti.is_signed ? "rem" : "urem";
            break;
        default:
            set_error(ctx, e->loc, "Unsupported binary operator '%s'",
                      token_type_name(e->binary.op));
            return v;
        }

        char tmp[64];
        new_temp(ctx, tmp, sizeof(tmp));
        sb_printf(&ctx->text_sb, "    %s =%s %s %s, %s\n", tmp, cls_str(common_ti.cls), instr,
                  left.repr, right.repr);

        snprintf(v.repr, sizeof(v.repr), "%s", tmp);
        v.ti = common_ti;
        v.is_const = false;
        return v;
    }

    case EXPR_CALL:
        return gen_call(ctx, e, false);

    default:
        set_error(ctx, e->loc, "Unsupported expression in code generator");
        return v;
    }

    return v;
}

static void gen_stmt(CodegenCtx *ctx, const AstStmt *s);

static void gen_stmt(CodegenCtx *ctx, const AstStmt *s) {
    assert(ctx && s);
    if (ctx->result->had_error) {
        return;
    }

    if (ctx->block_terminated) {
        return;
    }

    ctx->loc = s->loc;

    switch (s->kind) {
    case STMT_VAR_DECL: {
        TypeInfo ti = get_type_info(s->var_decl.type);
        if (ti.kind == TYPE_VOID) {
            set_error(ctx, s->loc, "A variable cannot have type void");
            return;
        }

        Symbol *existing = scope_find_current(&ctx->scope, s->var_decl.name);
        if (existing) {
            set_error(ctx, s->loc, "Redeclaration of variable '%s' in the same scope",
                      s->var_decl.name);
            return;
        }

        /* 1. Evaluate RHS initializer BEFORE adding variable to scope,
              so RHS can reference shadowed outer variable if needed.  */
        GenVal init_val;
        bool has_init = (s->var_decl.init_expr != NULL);
        if (has_init) {
            init_val = gen_expr(ctx, s->var_decl.init_expr);
            if (ctx->result->had_error) {
                return;
            }
        }

        char slot[64];
        scope_add(&ctx->scope, s->var_decl.name, ti, slot, sizeof(slot));

        if (ti.size_bytes <= 4) {
            sb_printf(&ctx->alloc_sb, "    %s =l alloc4 4\n", slot);
        } else {
            sb_printf(&ctx->alloc_sb, "    %s =l alloc8 8\n", slot);
        }

        if (has_init) {
            emit_store(ctx, init_val, slot, ti);
        } else {
            GenVal zero;
            memset(&zero, 0, sizeof(zero));
            if (ti.is_float) {
                snprintf(zero.repr, sizeof(zero.repr), (ti.cls == QBE_CLS_D) ? "d_0.0" : "s_0.0");
            } else {
                snprintf(zero.repr, sizeof(zero.repr), "0");
            }
            zero.ti = ti;
            zero.is_const = true;
            emit_store(ctx, zero, slot, ti);
        }
        break;
    }

    case STMT_ASSIGN: {
        if (s->assign.target->kind == EXPR_UNARY && s->assign.target->unary.op == TOK_ASTERISK) {
            GenVal ptr = gen_expr(ctx, s->assign.target->unary.operand);
            GenVal val = gen_expr(ctx, s->assign.value);

            TypeInfo elem_ti = get_pointee_type(ptr.ti);
            if (!ptr.ti.is_pointer || elem_ti.kind == TYPE_VOID) {
                set_error(ctx, s->loc, "Cannot store through an untyped or non-pointer value");
                return;
            }
            emit_store(ctx, val, ptr.repr, elem_ti);
            break;
        }

        if (s->assign.target->kind == EXPR_IDENTIFIER) {
            const char *var_name = s->assign.target->ident_name;
            Symbol *sym = scope_find(&ctx->scope, var_name);
            if (!sym) {
                set_error(ctx, s->loc, "Assignment to undeclared variable '%s'", var_name);
                return;
            }
            GenVal val = gen_expr(ctx, s->assign.value);
            emit_store(ctx, val, sym->slot, sym->type_info);

            sym->is_moved = false;
            break;
        }

        set_error(ctx, s->loc, "Invalid left-hand side in assignment expression");
        break;
    }

    case STMT_MOVE: {
        bool is_null_target =
            (strcmp(s->move.dst, "Null") == 0 || strcmp(s->move.dst, "null") == 0 ||
             strcmp(s->move.dst, "NULL") == 0);

        if (s->move.src_expr->kind == EXPR_IDENTIFIER) {
            const char *src_name = s->move.src_expr->ident_name;
            Symbol *src = scope_find(&ctx->scope, src_name);
            if (!src) {
                set_error(ctx, s->loc, "Unknown source variable '%s' in '>>' move operation",
                          src_name);
                return;
            }
            if (src->is_moved) {
                set_error(ctx, s->loc,
                          "Ownership error: variable '%s' was already moved on line %d and cannot "
                          "be moved again",
                          src_name, src->moved_at_line);
                return;
            }

            if (is_null_target) {
                if (src->type_info.is_pointer) {
                    char tmp[64];
                    new_temp(ctx, tmp, sizeof(tmp));
                    emit_load(ctx, tmp, src->slot, src->type_info);
                    sb_printf(&ctx->text_sb, "    call $free(l %s)\n", tmp);

                    GenVal zero;
                    memset(&zero, 0, sizeof(zero));
                    snprintf(zero.repr, sizeof(zero.repr), "0");
                    zero.ti = src->type_info;
                    zero.is_const = true;
                    emit_store(ctx, zero, src->slot, src->type_info);
                }
                src->is_moved = true;
                src->moved_at_line = s->loc.line;
                src->moved_at_col = s->loc.col;
                break;
            }

            Symbol *dst = scope_find(&ctx->scope, s->move.dst);
            if (!dst) {
                set_error(ctx, s->loc, "Unknown destination variable '%s' in '>>' move operation",
                          s->move.dst);
                return;
            }

            char tmp[64];
            new_temp(ctx, tmp, sizeof(tmp));
            emit_load(ctx, tmp, src->slot, src->type_info);

            GenVal val;
            memset(&val, 0, sizeof(val));
            snprintf(val.repr, sizeof(val.repr), "%s", tmp);
            val.ti = src->type_info;
            val.is_const = false;

            emit_store(ctx, val, dst->slot, dst->type_info);

            src->is_moved = true;
            src->moved_at_line = s->loc.line;
            src->moved_at_col = s->loc.col;
            dst->is_moved = false;
            break;
        } else {
            GenVal val = gen_expr(ctx, s->move.src_expr);
            if (ctx->result->had_error) {
                return;
            }

            if (is_null_target) {
                if (val.ti.is_pointer) {
                    sb_printf(&ctx->text_sb, "    call $free(l %s)\n", val.repr);
                }
                break;
            }

            Symbol *dst = scope_find(&ctx->scope, s->move.dst);
            if (!dst) {
                set_error(ctx, s->loc, "Unknown destination variable '%s' in '>>' move operation",
                          s->move.dst);
                return;
            }

            emit_store(ctx, val, dst->slot, dst->type_info);
            dst->is_moved = false;
            break;
        }
    }

    case STMT_IF: {
        int lbl_id = new_label_id(ctx);
        char lbl_then[32], lbl_else[32], lbl_end[32];
        snprintf(lbl_then, sizeof(lbl_then), "if_then_%d", lbl_id);
        snprintf(lbl_else, sizeof(lbl_else), "if_else_%d", lbl_id);
        snprintf(lbl_end, sizeof(lbl_end), "if_end_%d", lbl_id);

        GenVal cond = gen_expr(ctx, s->if_stmt.cond);
        cond = emit_to_bool(ctx, cond);

        if (s->if_stmt.else_count > 0) {
            sb_printf(&ctx->text_sb, "    jnz %s, @%s, @%s\n", cond.repr, lbl_then, lbl_else);
        } else {
            sb_printf(&ctx->text_sb, "    jnz %s, @%s, @%s\n", cond.repr, lbl_then, lbl_end);
        }

        size_t scope_cnt = ctx->scope.count;
        bool *saved_moves = scope_cnt ? xmalloc(scope_cnt * sizeof(*saved_moves)) : NULL;
        for (size_t i = 0; i < scope_cnt; i++) {
            saved_moves[i] = ctx->scope.symbols[i].is_moved;
        }

        sb_printf(&ctx->text_sb, "@%s\n", lbl_then);
        ctx->block_terminated = false;
        scope_enter(&ctx->scope);
        for (size_t i = 0; i < s->if_stmt.then_count; i++) {
            gen_stmt(ctx, s->if_stmt.then_stmts[i]);
        }
        scope_exit(&ctx->scope);
        bool then_terminated = ctx->block_terminated;

        bool *then_moves = scope_cnt ? xmalloc(scope_cnt * sizeof(*then_moves)) : NULL;
        for (size_t i = 0; i < scope_cnt; i++) {
            then_moves[i] = ctx->scope.symbols[i].is_moved;
        }

        if (!then_terminated) {
            sb_printf(&ctx->text_sb, "    jmp @%s\n", lbl_end);
        }

        bool else_terminated = false;
        bool *else_moves = scope_cnt ? xmalloc(scope_cnt * sizeof(*else_moves)) : NULL;
        if (s->if_stmt.else_count > 0) {
            for (size_t i = 0; i < scope_cnt; i++) {
                ctx->scope.symbols[i].is_moved = saved_moves[i];
            }

            sb_printf(&ctx->text_sb, "@%s\n", lbl_else);
            ctx->block_terminated = false;
            scope_enter(&ctx->scope);
            for (size_t i = 0; i < s->if_stmt.else_count; i++) {
                gen_stmt(ctx, s->if_stmt.else_stmts[i]);
            }
            scope_exit(&ctx->scope);
            else_terminated = ctx->block_terminated;

            for (size_t i = 0; i < scope_cnt; i++) {
                else_moves[i] = ctx->scope.symbols[i].is_moved;
            }

            if (!else_terminated) {
                sb_printf(&ctx->text_sb, "    jmp @%s\n", lbl_end);
            }
        } else {
            for (size_t i = 0; i < scope_cnt; i++) {
                else_moves[i] = saved_moves[i];
            }
        }

        /* Merge ownership states across branches.  */
        /* DBG("merge moves: then=%d else=%d", then_terminated, else_terminated);  */
        if (then_terminated && (s->if_stmt.else_count > 0 && else_terminated)) {
            ctx->block_terminated = true;
        } else {
            ctx->block_terminated = false;
            sb_printf(&ctx->text_sb, "@%s\n", lbl_end);

            for (size_t i = 0; i < scope_cnt; i++) {
                if (then_terminated) {
                    ctx->scope.symbols[i].is_moved = else_moves[i];
                } else if (s->if_stmt.else_count > 0 && else_terminated) {
                    ctx->scope.symbols[i].is_moved = then_moves[i];
                } else {
                    if (then_moves[i] || else_moves[i]) {
                        ctx->scope.symbols[i].is_moved = true;
                        ctx->scope.symbols[i].moved_at_line = s->loc.line;
                        ctx->scope.symbols[i].moved_at_col = s->loc.col;
                    } else {
                        ctx->scope.symbols[i].is_moved = saved_moves[i];
                    }
                }
            }
        }

        free(saved_moves);
        free(then_moves);
        free(else_moves);
        break;
    }

    case STMT_RETURN: {
        if (s->return_stmt.expr) {
            GenVal val = gen_expr(ctx, s->return_stmt.expr);
            if (ctx->ret_ti.kind == TYPE_VOID) {
                if (val.ti.kind != TYPE_VOID) {
                    set_error(ctx, s->loc, "A void function cannot return a value");
                    return;
                }
                sb_printf(&ctx->text_sb, "    ret\n");
            } else {
                val = coerce(ctx, val, ctx->ret_ti);
                sb_printf(&ctx->text_sb, "    ret %s\n", val.repr);
            }
        } else {
            if (ctx->ret_ti.kind == TYPE_VOID) {
                sb_printf(&ctx->text_sb, "    ret\n");
            } else if (ctx->ret_ti.is_float) {
                sb_printf(&ctx->text_sb, "    ret %s_0.0\n",
                          (ctx->ret_ti.cls == QBE_CLS_D) ? "d" : "s");
            } else {
                sb_printf(&ctx->text_sb, "    ret 0\n");
            }
        }
        ctx->block_terminated = true;
        break;
    }

    case STMT_EXPR:
        gen_expr(ctx, s->expr_stmt);
        break;
    }
}

typedef struct {
    char *name;
    TypeInfo ti;
} LocalVar;

typedef struct {
    LocalVar *vars;
    size_t count;
    size_t cap;
} LocalVarList;

static void local_var_add(LocalVarList *list, const char *name, TypeInfo ti) {
    assert(list && name);
    da_grow(list->vars, list->count, list->cap);
    list->vars[list->count].name = xstrdup(name);
    list->vars[list->count].ti = ti;
    list->count++;
}

static bool local_var_find(const LocalVarList *list, const char *name, TypeInfo *out) {
    assert(list && name);
    for (size_t i = list->count; i > 0; i--) {
        if (strcmp(list->vars[i - 1].name, name) == 0) {
            if (out) {
                *out = list->vars[i - 1].ti;
            }
            return true;
        }
    }
    return false;
}

static void local_var_free(LocalVarList *list) {
    assert(list);
    for (size_t i = 0; i < list->count; i++) {
        free(list->vars[i].name);
    }
    free(list->vars);
    list->vars = NULL;
    list->count = list->cap = 0;
}

static void collect_block_vars(const AstStmt *s, LocalVarList *list) {
    assert(s && list);
    if (s->kind == STMT_VAR_DECL) {
        local_var_add(list, s->var_decl.name, get_type_info(s->var_decl.type));
    } else if (s->kind == STMT_IF) {
        for (size_t i = 0; i < s->if_stmt.then_count; i++) {
            collect_block_vars(s->if_stmt.then_stmts[i], list);
        }
        for (size_t i = 0; i < s->if_stmt.else_count; i++) {
            collect_block_vars(s->if_stmt.else_stmts[i], list);
        }
    }
}

static bool infer_expr_type(const AstExpr *e, const LocalVarList *vars, FuncTable *ft,
                            const char *module_name, TypeInfo *out_ti);

static bool infer_call_type(const AstExpr *e, const LocalVarList *vars, FuncTable *ft,
                            const char *module_name, bool foreign_call, TypeInfo *out_ti) {
    if (!foreign_call && is_module_directive(e->call.callee)) {
        *out_ti = get_type_info_kind(TYPE_VOID);
        return true;
    }
    if (!foreign_call && is_syscall_target(e->call.callee)) {
        *out_ti = get_type_info_kind(TYPE_I64);
        return true;
    }
    if (!foreign_call && e->call.callee->kind == EXPR_IDENTIFIER &&
        strcmp(e->call.callee->ident_name, "ExternC") == 0) {
        if (e->call.arg_count != 1 || e->call.args[0]->kind != EXPR_CALL) {
            return false;
        }
        return infer_call_type(e->call.args[0], vars, ft, module_name, true, out_ti);
    }

    const char *target_module, *name;
    if (!call_target(e->call.callee, &target_module, &name)) {
        return false;
    }
    TypeInfo *args = e->call.arg_count ? xmalloc(e->call.arg_count * sizeof(*args)) : NULL;
    for (size_t i = 0; i < e->call.arg_count; i++) {
        if (!infer_expr_type(e->call.args[i], vars, ft, module_name, &args[i])) {
            free(args);
            return false;
        }
    }
    FuncSignature *fn;
    FuncResolution resolution =
        func_table_resolve(ft, target_module, module_name, name, args, e->call.arg_count, &fn);
    free(args);
    if (resolution != FUNC_FOUND || fn->is_external != foreign_call) {
        return false;
    }
    *out_ti = fn->ret_ti;
    return true;
}

static bool infer_expr_type(const AstExpr *e, const LocalVarList *vars, FuncTable *ft,
                            const char *module_name, TypeInfo *out_ti) {
    assert(e && vars && out_ti);
    switch (e->kind) {
    case EXPR_INT_LIT:
        if (e->int_val > 2147483647L || e->int_val < -2147483648L) {
            *out_ti = get_type_info_kind(TYPE_I64);
        } else {
            *out_ti = get_type_info_kind(TYPE_INT);
        }
        return true;
    case EXPR_FLOAT_LIT:
        *out_ti = get_type_info_kind(TYPE_F64);
        return true;
    case EXPR_STRING_LIT:
        *out_ti = get_type_info_kind(TYPE_STR);
        return true;
    case EXPR_BOOL_LIT:
        *out_ti = get_type_info_kind(TYPE_BOOL);
        return true;
    case EXPR_NULL_LIT:
        *out_ti = get_type_info_kind(TYPE_POINTER);
        out_ti->is_null = true;
        return true;
    case EXPR_IDENTIFIER:
        return local_var_find(vars, e->ident_name, out_ti);
    case EXPR_UNARY:
        if (e->unary.op == TOK_AMPERSAND) {
            *out_ti = get_type_info_kind(TYPE_POINTER);
            if (e->unary.operand->kind == EXPR_IDENTIFIER) {
                TypeInfo op_ti;
                if (local_var_find(vars, e->unary.operand->ident_name, &op_ti)) {
                    out_ti->elem_kind = op_ti.kind;
                    out_ti->pointee = op_ti.ast_type;
                }
            }
            return true;
        }
        if (e->unary.op == TOK_ASTERISK) {
            TypeInfo ptr_ti;
            if (infer_expr_type(e->unary.operand, vars, ft, module_name, &ptr_ti)) {
                *out_ti = get_pointee_type(ptr_ti);
                return true;
            }
            *out_ti = get_type_info_kind(TYPE_INT);
            return true;
        }
        if (e->unary.op == TOK_NOT) {
            *out_ti = get_type_info_kind(TYPE_BOOL);
            return true;
        }
        return infer_expr_type(e->unary.operand, vars, ft, module_name, out_ti);
    case EXPR_CALL:
        return infer_call_type(e, vars, ft, module_name, false, out_ti);
    case EXPR_BINARY:
        if (e->binary.op >= TOK_EQUAL && e->binary.op <= TOK_LESS_EQUAL) {
            *out_ti = get_type_info_kind(TYPE_BOOL);
            return true;
        }
        if (e->binary.op == TOK_AND || e->binary.op == TOK_OR) {
            *out_ti = get_type_info_kind(TYPE_BOOL);
            return true;
        }
        {
            TypeInfo l_ti = {0}, r_ti = {0};
            bool hl = infer_expr_type(e->binary.left, vars, ft, module_name, &l_ti);
            bool hr = infer_expr_type(e->binary.right, vars, ft, module_name, &r_ti);
            if ((hl && l_ti.is_float) || (hr && r_ti.is_float)) {
                *out_ti = get_type_info_kind(
                    (l_ti.cls == QBE_CLS_D || r_ti.cls == QBE_CLS_D) ? TYPE_F64 : TYPE_F32);
                return true;
            }
            if ((hl && l_ti.cls == QBE_CLS_L) || (hr && r_ti.cls == QBE_CLS_L)) {
                bool is_signed = (!hl || l_ti.is_signed) && (!hr || r_ti.is_signed);
                *out_ti = get_type_info_kind(is_signed ? TYPE_I64 : TYPE_U64);
                return true;
            }
            bool is_signed = (!hl || l_ti.is_signed) && (!hr || r_ti.is_signed);
            *out_ti = get_type_info_kind(is_signed ? TYPE_I32 : TYPE_U32);
            return true;
        }
    default:
        return false;
    }
}

static bool collect_returns_in_stmt(const AstStmt *s, const LocalVarList *vars, FuncTable *ft,
                                    const char *module_name, TypeInfo *returns, size_t *ret_count,
                                    size_t max_returns) {
    assert(s && vars && returns && ret_count);
    if (s->kind == STMT_RETURN) {
        if (*ret_count < max_returns) {
            TypeInfo ti = get_type_info_kind(TYPE_VOID);
            if (s->return_stmt.expr) {
                infer_expr_type(s->return_stmt.expr, vars, ft, module_name, &ti);
            }
            returns[(*ret_count)++] = ti;
        }
        return true;
    }
    if (s->kind == STMT_IF) {
        for (size_t i = 0; i < s->if_stmt.then_count; i++) {
            collect_returns_in_stmt(s->if_stmt.then_stmts[i], vars, ft, module_name, returns,
                                    ret_count, max_returns);
        }
        for (size_t i = 0; i < s->if_stmt.else_count; i++) {
            collect_returns_in_stmt(s->if_stmt.else_stmts[i], vars, ft, module_name, returns,
                                    ret_count, max_returns);
        }
    }
    return true;
}

static TypeInfo infer_return_type(const AstBlock *b, FuncTable *ft, CodegenResult *res) {
    assert(b && ft);
    if (b->ret_type != NULL) {
        return get_type_info(b->ret_type);
    }

    LocalVarList vars = {0, 0, 0};
    for (size_t p = 0; p < b->param_count; p++) {
        local_var_add(&vars, b->params[p]->name, get_type_info(b->params[p]->type));
    }
    for (size_t i = 0; i < b->stmt_count; i++) {
        collect_block_vars(b->stmts[i], &vars);
    }

    TypeInfo ret_types[32];
    size_t ret_count = 0;
    for (size_t i = 0; i < b->stmt_count; i++) {
        collect_returns_in_stmt(b->stmts[i], &vars, ft, b->module_name, ret_types, &ret_count, 32);
    }

    local_var_free(&vars);

    if (ret_count == 0) {
        return get_type_info_kind(TYPE_INT);
    }

    /* Verify compatibility across all return branches.  */
    TypeInfo first = ret_types[0];
    for (size_t i = 1; i < ret_count; i++) {
        TypeInfo cur = ret_types[i];
        if ((cur.kind == TYPE_VOID) != (first.kind == TYPE_VOID) || cur.cls != first.cls ||
            cur.is_float != first.is_float) {
            /* Allow literal 0 as NULL pointer across pointer returns.  */
            if (first.is_pointer && cur.cls == QBE_CLS_W && cur.kind != TYPE_VOID) {
                continue;
            }
            if (cur.is_pointer && first.cls == QBE_CLS_W && first.kind != TYPE_VOID) {
                first = cur;
                continue;
            }

            if (res && !res->had_error) {
                res->had_error = true;
                res->error_line = 1;
                res->error_column = 1;
                snprintf(res->error_msg, sizeof(res->error_msg),
                         "Return type mismatch in function '%s': incompatible return types across "
                         "branches",
                         b->name);
            }
            return first;
        }
    }

    return first;
}

/* Generate complete QBE intermediate code string from AST PROG.  */
char *codegen_generate_qbe(const AstProgram *prog, CodegenResult *res) {
    assert(res);
    res->had_error = false;
    res->error_msg[0] = '\0';

    if (!prog || prog->block_count == 0) {
        res->had_error = true;
        snprintf(res->error_msg, sizeof(res->error_msg), "Empty program: no code blocks found");
        return NULL;
    }

    bool has_init = false;
    bool has_main = false;
    size_t entry_count = 0;
    for (size_t i = 0; i < prog->block_count; i++) {
        AstBlock *b = prog->blocks[i];
        if (strcmp(b->module_name, prog->module_name) != 0) {
            continue;
        }
        if (strcmp(b->name, "Init") == 0) {
            has_init = true;
            entry_count++;
        }
        if (strcmp(b->name, "main") == 0) {
            has_main = true;
            entry_count++;
        }
    }
    if (entry_count > 1) {
        res->had_error = true;
        res->error_line = 1;
        res->error_column = 1;
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "Conflicting entry points: define exactly one 'Init' or 'main' block");
        return NULL;
    }
    if (!has_init && !has_main) {
        res->had_error = true;
        res->error_line = 1;
        res->error_column = 1;
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "Program lacks entry point: block -[Init] is required");
        return NULL;
    }

    CodegenCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.result = res;
    ctx.temp_counter = 1;
    ctx.label_counter = 1;

    sb_init(&ctx.data_sb);
    sb_init(&ctx.alloc_sb);
    sb_init(&ctx.text_sb);
    pool_init(&ctx.pool);
    func_table_init(&ctx.func_table);

    /* Register every signature before inferring calls to later declarations.  */
    for (size_t i = 0; i < prog->block_count; i++) {
        AstBlock *b = prog->blocks[i];
        TypeInfo rti = b->ret_type ? get_type_info(b->ret_type) : get_type_info_kind(TYPE_INT);
        bool entry = strcmp(b->module_name, prog->module_name) == 0 &&
                     (strcmp(b->name, "Init") == 0 || strcmp(b->name, "main") == 0);
        if (entry) {
            rti = get_type_info_kind(TYPE_INT);
        }
        for (size_t p = 0; p < b->param_count; p++) {
            if (b->params[p]->type->kind == TYPE_VOID) {
                set_error(&ctx, loc_make(1, 1), "A function parameter cannot have type void");
                break;
            }
        }
        if (res->had_error) {
            break;
        }
        if (!func_table_add(&ctx.func_table, b->module_name, b->name, rti, b->params,
                            b->param_count, false, entry)) {
            set_error(&ctx, loc_make(1, 1),
                      "Redeclaration of function '%s.%s' with the same parameters", b->module_name,
                      b->name);
            break;
        }
    }
    for (size_t i = 0; i < prog->extern_block_count && !res->had_error; i++) {
        AstExternBlock *eb = prog->extern_blocks[i];
        TypeInfo rti = eb->ret_type ? get_type_info(eb->ret_type) : get_type_info_kind(TYPE_INT);
        for (size_t p = 0; p < eb->param_count; p++) {
            if (eb->params[p]->type->kind == TYPE_VOID) {
                set_error(&ctx, eb->loc, "A function parameter cannot have type void");
                break;
            }
        }
        if (res->had_error) {
            break;
        }
        if (!func_table_add(&ctx.func_table, eb->module_name, eb->name, rti, eb->params,
                            eb->param_count, true, false)) {
            set_error(&ctx, eb->loc, "Redeclaration of external function '%s'", eb->name);
        }
    }

    /* Propagate return types through forward calls until the signatures settle.  */
    for (size_t pass = 0; pass <= prog->block_count && !res->had_error; pass++) {
        bool changed = false;
        for (size_t i = 0; i < prog->block_count; i++) {
            AstBlock *b = prog->blocks[i];
            FuncSignature *sig = &ctx.func_table.funcs[i];
            if (!b->ret_type && strcmp(sig->mangled_name, "main") != 0) {
                TypeInfo rti = infer_return_type(b, &ctx.func_table, NULL);
                changed = changed || !type_info_equal(sig->ret_ti, rti);
                sig->ret_ti = rti;
            }
        }
        if (!changed) {
            break;
        }
    }
    for (size_t i = 0; i < prog->block_count && !res->had_error; i++) {
        if (strcmp(ctx.func_table.funcs[i].mangled_name, "main") != 0) {
            infer_return_type(prog->blocks[i], &ctx.func_table, res);
        }
    }
    if (res->had_error) {
        func_table_free(&ctx.func_table);
        pool_free(&ctx.pool);
        sb_free(&ctx.data_sb);
        sb_free(&ctx.alloc_sb);
        sb_free(&ctx.text_sb);
        return NULL;
    }

    StrBuf full_text;
    sb_init(&full_text);

    for (size_t i = 0; i < prog->block_count; i++) {
        AstBlock *b = prog->blocks[i];
        scope_init(&ctx.scope);

        ctx.alloc_sb.len = 0;
        if (ctx.alloc_sb.data) {
            ctx.alloc_sb.data[0] = '\0';
        }
        ctx.text_sb.len = 0;
        if (ctx.text_sb.data) {
            ctx.text_sb.data[0] = '\0';
        }
        ctx.block_terminated = false;

        FuncSignature *sig = &ctx.func_table.funcs[i];
        ctx.module_name = b->module_name;
        ctx.ret_ti = sig->ret_ti;
        const char *func_qbe_name = sig->mangled_name;

        for (size_t p = 0; p < b->param_count; p++) {
            TypeInfo pti = get_type_info(b->params[p]->type);
            char slot[64];
            scope_add(&ctx.scope, b->params[p]->name, pti, slot, sizeof(slot));

            int align = (pti.size_bytes >= 8) ? 8 : 4;
            int alloc_sz = (pti.size_bytes >= 8) ? 8 : 4;
            sb_printf(&ctx.alloc_sb, "    %s =l alloc%d %d\n", slot, align, alloc_sz);

            GenVal pval;
            memset(&pval, 0, sizeof(pval));
            snprintf(pval.repr, sizeof(pval.repr), "%%arg_%zu", p);
            pval.ti = pti;
            pval.is_const = false;
            emit_store(&ctx, pval, slot, pti);
        }

        for (size_t j = 0; j < b->stmt_count; j++) {
            gen_stmt(&ctx, b->stmts[j]);
            if (ctx.result->had_error) {
                scope_free(&ctx.scope);
                func_table_free(&ctx.func_table);
                pool_free(&ctx.pool);
                sb_free(&ctx.data_sb);
                sb_free(&ctx.alloc_sb);
                sb_free(&ctx.text_sb);
                sb_free(&full_text);
                return NULL;
            }
        }

        if (!ctx.block_terminated) {
            if (ctx.ret_ti.kind == TYPE_VOID) {
                sb_printf(&ctx.text_sb, "    ret\n");
            } else if (ctx.ret_ti.is_float) {
                sb_printf(&ctx.text_sb, "    ret %s_0.0\n",
                          (ctx.ret_ti.cls == QBE_CLS_D) ? "d" : "s");
            } else {
                sb_printf(&ctx.text_sb, "    ret 0\n");
            }
        }

        if (ctx.ret_ti.kind == TYPE_VOID) {
            sb_printf(&full_text, "export function $%s(", func_qbe_name);
        } else {
            sb_printf(&full_text, "export function %s $%s(", cls_str(ctx.ret_ti.cls),
                      func_qbe_name);
        }
        for (size_t p = 0; p < b->param_count; p++) {
            TypeInfo pti = get_type_info(b->params[p]->type);
            if (p > 0) {
                sb_printf(&full_text, ", ");
            }
            sb_printf(&full_text, "%s %%arg_%zu", cls_str(pti.cls), p);
        }
        sb_printf(&full_text, ") {\n@start\n%s%s}\n\n", ctx.alloc_sb.data, ctx.text_sb.data);

        scope_free(&ctx.scope);
    }

    func_table_free(&ctx.func_table);

    for (size_t i = 0; i < ctx.pool.count; i++) {
        sb_printf(&ctx.data_sb, "data $%s = { b \"%s\", b 0 }\n", ctx.pool.items[i].label,
                  ctx.pool.items[i].text);
    }
    pool_free(&ctx.pool);

    StrBuf full;
    sb_init(&full);
    sb_printf(&full, "%s\n%s", ctx.data_sb.data, full_text.data);

    sb_free(&ctx.data_sb);
    sb_free(&ctx.alloc_sb);
    sb_free(&ctx.text_sb);
    sb_free(&full_text);

    return full.data;
}
