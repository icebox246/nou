#ifndef PARSE_H_
#define PARSE_H_

#include <stdbool.h>

#include "da.h"
#include "lex.h"

// exports

typedef struct {
    char* decl_name;
} Export;

typedef struct {
    da_list(Export);
} Exports;

// decls

typedef enum {
    DK_FUNCTION,
    DK_EXTERN_FUNCTION,
    DK_PARAM,
    DK_VARIABLE,
} DeclKind;

typedef enum {
    VT_NIL,
    VT_INT,
    VT_BOOL,
    VT_SLICE,
} ValueTypeKind;

typedef struct ValueType {
    ValueTypeKind kind;
    union {
        struct {
            int bits;
            bool unsign;
        } i;
        struct ValueType* inner_type;
    } props;
} ValueType;

typedef struct {
    char* name;
    DeclKind kind;
    union {
        ValueType vt;
        size_t func_index;
    } value;
} Decl;

typedef struct {
    da_list(Decl);
    size_t parent;
    bool param_scope;
} DeclScope;

typedef struct {
    da_list(DeclScope);
} DeclScopes;

// expressions

typedef enum {
    EK_INT_CONST,
    EK_BOOL_CONST,
    EK_STRING_CONST,
    EK_VAR,
    EK_OPERATOR,
    EK_FUNC_CALL,
    EK_FIELD_ACCESS,
    EK_CASTING,
} ExprKind;

typedef enum {
    OP_ADDITION,
    OP_SUBTRACTION,
    OP_MULTIPLICATION,
    OP_REMAINDER,
    OP_DIVISION,
    OP_ASSIGNEMENT,
    OP_EQUALITY,
    OP_ALTERNATIVE,
    OP_CONJUNCTION,
    OP_INDEXING,

    OP_OPEN_PAREN,
    OP_FUNC_CALL,
    OP_FIELD_ACCESS,
    OP_CASTING,
} OperatorKind;

typedef struct {
    ExprKind kind;
    union {
        struct {
            int64_t value;
            int bits;
            bool unsign;
        } i;
        bool boolean;
        char* var;
        OperatorKind op;
        char* func;
        size_t str_index;
        char* field_name;
        ValueType cast_target;
    } props;
} Expr;

// expressions are stored as list of Expr which describe exression in RPN
typedef struct {
    da_list(Expr);
} Expression;

// statements

typedef enum {
    SK_EMPTY,
    SK_BLOCK,
    SK_RETURN,
    SK_IF,
    SK_EXPRESSION,
} StatementKind;

typedef struct {
    StatementKind kind;
    Expression expr;
} ReturnStatement;

typedef struct {
    StatementKind kind;
    Expression expr;
} ExpressionStatement;

typedef union Statement {
    StatementKind kind;
    struct BlockStatement {
        StatementKind kind;
        size_t scope;
        da_list(union Statement);
    } block;
    ReturnStatement ret;
    struct IfStatement {
        StatementKind kind;
        Expression cond_expr;
        union Statement* positive_branch;
        union Statement* negative_branch;
    } ifs;
    ExpressionStatement expr;
} Statement;

typedef struct BlockStatement BlockStatement;

typedef struct IfStatement IfStatement;

// function types

typedef struct {
    size_t param_scope;
    ValueType return_type;
} FunctionType;

typedef struct {
    da_list(FunctionType);
} FunctionTypes;

// functions

typedef struct {
    size_t param_scope;
    ValueType return_type;
    Statement content;
    size_t function_type;
} Function;

typedef struct {
    da_list(Function);
} Functions;

// string literals

typedef struct {
    char* chars;
    size_t len;
} StringConstant;

typedef struct {
    da_list(StringConstant);
} StringConstants;

// module

typedef struct {
    Exports exports;
    DeclScopes scopes;
    FunctionTypes function_types;
    Functions extern_functions;
    Functions functions;
    StringConstants string_constants;
} Module;

typedef struct {
    Lexer* lex;
    Module* mod;
    size_t current_scope;  // 0 is global scope
} Parser;

bool compare_value_types(ValueType a, ValueType b);

Module parse(Lexer* lexer);

#endif
