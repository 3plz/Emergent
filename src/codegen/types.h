#ifndef EMC_CODEGEN_TYPES_H
#define EMC_CODEGEN_TYPES_H

#include "../parser/ast.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    QBE_CLS_W, /* 32-bit word.  */
    QBE_CLS_L, /* 64-bit long.  */
    QBE_CLS_S, /* Single float.  */
    QBE_CLS_D  /* Double float.  */
} QbeCls;

typedef struct {
    QbeCls cls;
    bool is_signed;
    bool is_float;
    bool is_pointer;
    bool is_null;
    int size_bytes;
    AstTypeKind kind;
    AstTypeKind elem_kind;
    const AstType *ast_type;
    const AstType *pointee;
} TypeInfo;

typedef struct {
    char repr[256];
    TypeInfo ti;
    bool is_const;
} GenVal;

TypeInfo get_type_info_kind(AstTypeKind kind);
TypeInfo get_type_info(const AstType *t);
TypeInfo get_pointee_type(TypeInfo ti);
bool type_info_equal(TypeInfo a, TypeInfo b);
int type_conversion_cost(TypeInfo from, TypeInfo to);
const char *cls_str(QbeCls cls);
bool is_int_literal(const char *s);

#endif
