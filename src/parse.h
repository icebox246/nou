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
    DK_PARAM,
    DK_VARIABLE,
} DeclKind;

typedef enum {
    VT_NIL,
    VT_I32,
} ValueType;

typedef struct {
    char* name;
    DeclKind kind;
    size_t value;
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
    EK_I32_CONST,
    EK_VAR,
    EK_OPERATOR,
} ExprKind;

typedef enum {
    OP_ADDITION,
    OP_SUBTRACTION,
    OP_MULTIPLICATION,
    OP_ASSIGNEMENT,
    OP_OPEN_PAREN,
} OperatorKind;

typedef struct {
    ExprKind kind;
    union {
        int32_t i32;
        char* var;
        OperatorKind op;
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
    ExpressionStatement expr;
} Statement;

typedef struct BlockStatement BlockStatement;

// functions

typedef struct {
    size_t param_scope;
    ValueType return_type;
    Statement content;
} Function;

typedef struct {
    da_list(Function);
} Functions;

// module

typedef struct {
    Exports exports;
    DeclScopes scopes;
    Functions functions;
} Module;

typedef struct {
    Lexer* lex;
    Module* mod;
    size_t current_scope;  // 0 is global scope
} Parser;

Module parse(Lexer* lexer);

#endif
