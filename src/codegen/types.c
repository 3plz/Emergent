#include "types.h"
#include <ctype.h>
#include <string.h>

TypeInfo get_type_info_kind(AstTypeKind kind) {
    TypeInfo ti;
    memset(&ti, 0, sizeof(ti));
    ti.kind = kind;

    switch (kind) {
    case TYPE_VOID:
        ti.cls = QBE_CLS_W;
        ti.size_bytes = 0;
        break;
    case TYPE_I8:
        ti.cls = QBE_CLS_W;
        ti.is_signed = true;
        ti.size_bytes = 1;
        break;
    case TYPE_I16:
        ti.cls = QBE_CLS_W;
        ti.is_signed = true;
        ti.size_bytes = 2;
        break;
    case TYPE_I32:
    case TYPE_INT:
        ti.cls = QBE_CLS_W;
        ti.is_signed = true;
        ti.size_bytes = 4;
        break;
    case TYPE_I64:
        ti.cls = QBE_CLS_L;
        ti.is_signed = true;
        ti.size_bytes = 8;
        break;
    case TYPE_U8:
        ti.cls = QBE_CLS_W;
        ti.size_bytes = 1;
        break;
    case TYPE_U16:
        ti.cls = QBE_CLS_W;
        ti.size_bytes = 2;
        break;
    case TYPE_U32:
    case TYPE_UINT:
        ti.cls = QBE_CLS_W;
        ti.size_bytes = 4;
        break;
    case TYPE_U64:
        ti.cls = QBE_CLS_L;
        ti.size_bytes = 8;
        break;
    case TYPE_F32:
        ti.cls = QBE_CLS_S;
        ti.is_float = true;
        ti.size_bytes = 4;
        break;
    case TYPE_F64:
        ti.cls = QBE_CLS_D;
        ti.is_float = true;
        ti.size_bytes = 8;
        break;
    case TYPE_STR:
        ti.cls = QBE_CLS_L;
        ti.is_pointer = true;
        ti.size_bytes = 8;
        break;
    case TYPE_BOOL:
        ti.cls = QBE_CLS_W;
        ti.size_bytes = 1;
        break;
    case TYPE_POINTER:
        ti.cls = QBE_CLS_L;
        ti.is_pointer = true;
        ti.size_bytes = 8;
        break;
    case TYPE_CUSTOM:
        ti.cls = QBE_CLS_W;
        ti.is_signed = true;
        ti.size_bytes = 4;
        break;
    default:
        ti.cls = QBE_CLS_W;
        ti.is_signed = true;
        ti.size_bytes = 4;
        break;
    }
    return ti;
}

TypeInfo get_type_info(const AstType *t) {
    if (!t) {
        return get_type_info_kind(TYPE_INT);
    }
    TypeInfo ti = get_type_info_kind(t->kind);
    ti.ast_type = t;
    if (t->kind == TYPE_POINTER && t->base_type) {
        ti.elem_kind = t->base_type->kind;
        ti.pointee = t->base_type;
    }
    return ti;
}

static AstTypeKind canonical_kind(AstTypeKind kind) {
    if (kind == TYPE_INT) {
        return TYPE_I32;
    }
    if (kind == TYPE_UINT) {
        return TYPE_U32;
    }
    return kind;
}

/* AST types retain pointer depth and custom names that QBE classes cannot express.  */
static bool ast_type_equal(const AstType *a, const AstType *b) {
    if (!a || !b) {
        return a == b;
    }
    if (canonical_kind(a->kind) != canonical_kind(b->kind)) {
        return false;
    }
    if (a->kind == TYPE_POINTER) {
        return ast_type_equal(a->base_type, b->base_type);
    }
    if (a->kind == TYPE_CUSTOM) {
        return strcmp(a->custom_name, b->custom_name) == 0;
    }
    return true;
}

TypeInfo get_pointee_type(TypeInfo ti) {
    if (ti.pointee) {
        return get_type_info(ti.pointee);
    }
    return get_type_info_kind(ti.elem_kind);
}

bool type_info_equal(TypeInfo a, TypeInfo b) {
    if (canonical_kind(a.kind) != canonical_kind(b.kind)) {
        return false;
    }
    if (a.kind == TYPE_POINTER) {
        return type_info_equal(get_pointee_type(a), get_pointee_type(b));
    }
    if (a.kind == TYPE_CUSTOM) {
        return ast_type_equal(a.ast_type, b.ast_type);
    }
    return true;
}

int type_conversion_cost(TypeInfo from, TypeInfo to) {
    if (from.kind == TYPE_VOID || to.kind == TYPE_VOID) {
        return -1;
    }
    if (type_info_equal(from, to)) {
        return 0;
    }
    if (from.is_null && to.is_pointer) {
        return 1;
    }
    if (from.is_pointer || to.is_pointer) {
        if (from.is_pointer && to.kind == TYPE_POINTER && get_pointee_type(to).kind == TYPE_VOID) {
            return 1;
        }
        return -1;
    }
    if (from.kind == TYPE_CUSTOM || to.kind == TYPE_CUSTOM) {
        return -1;
    }
    /* Prefer widening within one numeric family over other numeric conversions.  */
    if (from.is_float == to.is_float && from.is_signed == to.is_signed &&
        from.size_bytes < to.size_bytes && from.kind != TYPE_BOOL && to.kind != TYPE_BOOL) {
        return 1;
    }
    return 2;
}

const char *cls_str(QbeCls cls) {
    switch (cls) {
    case QBE_CLS_W:
        return "w";
    case QBE_CLS_L:
        return "l";
    case QBE_CLS_S:
        return "s";
    case QBE_CLS_D:
        return "d";
    }
    return "w";
}

bool is_int_literal(const char *s) {
    if (!s || !*s) {
        return false;
    }
    if (*s == '-' || *s == '+') {
        s++;
    }
    if (!*s) {
        return false;
    }
    while (*s) {
        if (!isdigit((unsigned char)*s)) {
            return false;
        }
        s++;
    }
    return true;
}
