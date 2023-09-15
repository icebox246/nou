#include "mod_vis.h"

#include <assert.h>
#include <stdbool.h>

typedef struct {
    FILE* file;
    size_t indent;
    Module* mod;
} Visualizer;

void vis_write_indent(Visualizer* v) {
    for (size_t i = 0; i < v->indent; i++) {
        fprintf(v->file, "  ");
    }
}

// value type

void visualize_value_type(ValueType vt, Visualizer* v) {
    switch (vt.kind) {
        case VT_NIL:
            fprintf(v->file, "nil");
            break;
        case VT_INT:
            fprintf(v->file, "i%d", vt.props.i.bits);
            break;
        case VT_BOOL:
            fprintf(v->file, "bool");
            break;
    }
}

// expressions

void visualize_expr(Expr* e, Visualizer* v) {
    switch (e->kind) {
        case EK_INT_CONST:
            vis_write_indent(v);
            fprintf(v->file, "int_const %zi\n", e->props.i.value);
            break;
        case EK_BOOL_CONST:
            vis_write_indent(v);
            fprintf(v->file, "bool_const %s\n",
                    e->props.boolean ? "true" : "false");
            break;
        case EK_VAR:
            vis_write_indent(v);
            fprintf(v->file, "var %s\n", e->props.var);
            break;
        case EK_OPERATOR:
            vis_write_indent(v);
            fprintf(v->file, "op ");
            switch (e->props.op) {
                case OP_ADDITION:
                    fprintf(v->file, "+");
                    break;
                case OP_SUBTRACTION:
                    fprintf(v->file, "-");
                    break;
                case OP_MULTIPLICATION:
                    fprintf(v->file, "*");
                    break;
                case OP_REMAINDER:
                    fprintf(v->file, "%%");
                    break;
                case OP_DIVISION:
                    fprintf(v->file, "/");
                    break;
                case OP_ASSIGNEMENT:
                    fprintf(v->file, "=");
                    break;
                case OP_EQUALITY:
                    fprintf(v->file, "==");
                    break;
                case OP_ALTERNATIVE:
                    fprintf(v->file, "or");
                    break;
                case OP_CONJUNCTION:
                    fprintf(v->file, "and");
                    break;

                case OP_OPEN_PAREN:
                case OP_FUNC_CALL:
                    assert(false && "Unreachable");
                    break;
            }
            fprintf(v->file, "\n");
            break;
        case EK_FUNC_CALL:
            vis_write_indent(v);
            fprintf(v->file, "call %s\n", e->props.func);
            break;
    }
}

void visualize_expression(Expression* e, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "expression {\n");

    v->indent++;
    for (size_t i = 0; i < e->count; i++) {
        visualize_expr(&e->items[i], v);
    }
    v->indent--;

    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

// statements

void visualize_statement(Statement* s, Visualizer* v);

void visualize_block_statement(BlockStatement* s, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "block {\n");

    v->indent++;
    vis_write_indent(v);
    fprintf(v->file, "scope: #%zu\n", s->scope);

    for (size_t i = 0; i < s->count; i++) {
        visualize_statement(&s->items[i], v);
    }
    v->indent--;

    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_return_statement(ReturnStatement* s, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "return {\n");

    v->indent++;
    visualize_expression(&s->expr, v);
    v->indent--;

    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_if_statement(IfStatement* s, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "if (\n");

    v->indent++;
    visualize_expression(&s->cond_expr, v);
    v->indent--;

    vis_write_indent(v);
    fprintf(v->file, ")\n");

    v->indent++;
    visualize_statement(s->positive_branch, v);
    v->indent--;

    if (s->negative_branch) {
        vis_write_indent(v);
        fprintf(v->file, "else\n");
        v->indent++;
        visualize_statement(s->negative_branch, v);
        v->indent--;
    }
}

void visualize_expr_statement(ReturnStatement* s, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "expr {\n");

    v->indent++;
    visualize_expression(&s->expr, v);
    v->indent--;

    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_statement(Statement* s, Visualizer* v) {
    switch (s->kind) {
        case SK_EMPTY:
            vis_write_indent(v);
            fprintf(v->file, ";\n");
        case SK_BLOCK:
            visualize_block_statement(&s->block, v);
            break;
        case SK_RETURN:
            visualize_return_statement(&s->ret, v);
            break;
        case SK_IF:
            visualize_if_statement(&s->ifs, v);
            break;
        case SK_EXPRESSION:
            visualize_expr_statement(&s->ret, v);
            break;
    }
}

// exports

void visualize_export(Export* export, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "export \"%s\" \n", export->decl_name);
}

void visualize_exports(Exports* exports, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "exports {\n");
    v->indent++;
    for (size_t i = 0; i < exports->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i);
        v->indent++;
        visualize_export(&exports->items[i], v);
        v->indent--;
    }
    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

// decls and scopes

void visualize_decl(Decl* decl, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "decl {");
    switch (decl->kind) {
        case DK_FUNCTION:
            fprintf(v->file, "%s := fn #%zu", decl->name,
                    decl->value.func_index + v->mod->extern_functions.count);
            break;
        case DK_EXTERN_FUNCTION:
            fprintf(v->file, "extern %s := fn #%zu", decl->name,
                    decl->value.func_index);
            break;
        case DK_PARAM:
            fprintf(v->file, "%s := param ", decl->name);
            visualize_value_type(decl->value.vt, v);
            break;
        case DK_VARIABLE:
            fprintf(v->file, "%s := var ", decl->name);
            visualize_value_type(decl->value.vt, v);
            break;
    }
    fprintf(v->file, "}\n");
}

void visualize_scope(DeclScope* scope, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "scope {\n");
    v->indent++;
    vis_write_indent(v);
    fprintf(v->file, "parent: #%zu\n", scope->parent);

    for (size_t i = 0; i < scope->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i);
        v->indent++;
        visualize_decl(&scope->items[i], v);
        v->indent--;
    }
    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_scopes(DeclScopes* scopes, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "scopes {\n");
    v->indent++;
    for (size_t i = 0; i < scopes->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i);
        v->indent++;
        visualize_scope(&scopes->items[i], v);
        v->indent--;
    }
    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

// function types

void visualize_function_type(FunctionType* function_type, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "fn type {\n");
    v->indent++;

    vis_write_indent(v);
    fprintf(v->file, "param_scope: #%zu\n", function_type->param_scope);

    vis_write_indent(v);
    fprintf(v->file, "return_type: ");
    visualize_value_type(function_type->return_type, v);
    fprintf(v->file, "\n");

    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_function_types(FunctionTypes* function_types, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "function types {\n");
    v->indent++;

    for (size_t i = 0; i < function_types->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i);
        v->indent++;
        visualize_function_type(&function_types->items[i], v);
        v->indent--;
    }

    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

// functions

void visualize_function(Function* function, Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "fn {\n");
    v->indent++;

    vis_write_indent(v);
    fprintf(v->file, "param_scope: #%zu\n", function->param_scope);

    vis_write_indent(v);
    fprintf(v->file, "return_type: ");
    visualize_value_type(function->return_type, v);
    fprintf(v->file, "\n");

    vis_write_indent(v);
    fprintf(v->file, "function_type: #%zu\n", function->function_type);

    vis_write_indent(v);
    fprintf(v->file, "content:\n");
    v->indent++;
    visualize_statement(&function->content, v);
    v->indent--;

    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

void visualize_functions(Functions* extern_functions, Functions* functions,
                         Visualizer* v) {
    vis_write_indent(v);
    fprintf(v->file, "extern functions {\n");
    v->indent++;
    for (size_t i = 0; i < extern_functions->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i);
        v->indent++;
        visualize_function(&extern_functions->items[i], v);
        v->indent--;
    }
    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n\n");

    vis_write_indent(v);
    fprintf(v->file, "functions {\n");
    v->indent++;
    for (size_t i = 0; i < functions->count; i++) {
        vis_write_indent(v);
        fprintf(v->file, "#%zu:\n", i + extern_functions->count);
        v->indent++;
        visualize_function(&functions->items[i], v);
        v->indent--;
    }
    v->indent--;
    vis_write_indent(v);
    fprintf(v->file, "}\n");
}

// module

void visualize_module(Module* mod, FILE* file) {
    Visualizer v = {.file = file, .mod = mod};

    vis_write_indent(&v);
    fprintf(file, "module {\n");

    fprintf(file, "\n");
    visualize_exports(&mod->exports, &v);

    fprintf(file, "\n");
    visualize_scopes(&mod->scopes, &v);

    fprintf(file, "\n");
    visualize_function_types(&mod->function_types, &v);

    fprintf(file, "\n");
    visualize_functions(&mod->extern_functions, &mod->functions, &v);

    fprintf(file, "\n");

    vis_write_indent(&v);
    fprintf(file, "}\n");
}
