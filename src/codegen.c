#include "codegen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SID_CUSTOM,
    SID_TYPE,
    SID_IMPORT,
    SID_FUNCTION,
    SID_TABLE,
    SID_MEMORY,
    SID_GLOBAL,
    SID_EXPORT,
    SID_START,
    SID_ELEMENT,
    SID_CODE,
    SID_DATA,
    SID_DATA_COUNT,
} SectionId;

typedef struct {
    SectionId id;
    ByteBuffer content;
} Section;

typedef struct {
    uint32_t count;
    ByteBuffer content;
} Vec;

typedef struct {
    ByteBuffer output_buffer;
} Generator;

static Decl* find_global_decl(Module* mod, const char* name) {
    DeclScope* s = &mod->scopes.items[0];

    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(name, s->items[i].name) == 0) {
            return &s->items[i];
        }
    }

    return NULL;
}

static void bb_append_bytes(ByteBuffer* bb, uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        da_append(*bb, bytes[i]);
    }
}

static void bb_append_bb(ByteBuffer* bb, ByteBuffer* other) {
    bb_append_bytes(bb, other->items, other->count);
}

static void bb_append_leb128_u(ByteBuffer* bb, uint64_t x) {
    do {
        uint8_t byte = (x & 0x7F);
        x >>= 7;
        if (x) byte |= 0x80;
        da_append(*bb, byte);
    } while (x);
}

static void bb_append_name(ByteBuffer* bb, char* name) {
    size_t len = strlen(name);
    bb_append_leb128_u(bb, len);
    bb_append_bytes(bb, (uint8_t*)name, len);
}

static void bb_append_section(ByteBuffer* bb, Section* s) {
    da_append(*bb, s->id);
    bb_append_leb128_u(bb, s->content.count);
    bb_append_bb(bb, &s->content);
}

static void bb_append_vec(ByteBuffer* bb, Vec* v) {
    bb_append_leb128_u(bb, v->count);
    bb_append_bb(bb, &v->content);
}

static void vec_append_elem(Vec* v, ByteBuffer* elem) {
    v->count++;
    bb_append_bb(&v->content, elem);
}

// value types

ByteBuffer codegen_value_type(Module* mod, ValueType vt) {
    switch (vt) {
        case VT_NIL:
            fprintf(stderr, "Nil value type should not be codegenned\n");
            exit(1);
            break;
        case VT_I32: {
            ByteBuffer bb = {0};
            da_append(bb, 0x7F);
            return bb;
        } break;
    }

    assert(false && "Unreachable");
}

// expressions

bool find_var_local_index(Module* mod, size_t scope, char* name,
                          size_t* out_index) {
    if (out_index) *out_index = 0;
    DeclScope* s = &mod->scopes.items[scope];

    if (scope != 0) {
        if (find_var_local_index(mod, s->parent, name, out_index)) return true;
    }

    for (size_t i = 0; i < s->count; i++) {
        Decl* decl = &s->items[i];
        if (strcmp(decl->name, name) == 0) return true;
        if (out_index && decl->kind == DK_VARIABLE) ++*out_index;
    }

    return false;
}

ByteBuffer codegen_expr(Module* mod, Expr* ex, size_t scope) {
    ByteBuffer e = {0};
    switch (ex->kind) {
        case EK_I32_CONST:
            da_append(e, 0x41);                     // opcode for i32.const
            bb_append_leb128_u(&e, ex->props.i32);  // TODO make it signed
            break;
        case EK_VAR: {
            da_append(e, 0x20);  // opcode for local.get
            size_t var_index;
            if (!find_var_local_index(mod, scope, ex->props.var, &var_index)) {
                // TODO probably function local variables should be stored in
                // linear memory
                fprintf(stderr, "Can't find `%s` used in expr\n",
                        ex->props.var);
                exit(1);
            }
            bb_append_leb128_u(&e, var_index);
        } break;
        case EK_OPERATOR: {
            switch (ex->props.op) {
                case OP_ADDITION:  // TODO figure out which instruction should
                                   // be codegenned base on type stack
                    da_append(e, 0x6A);  // opcode for i32.add
                    break;
            }
        } break;
    }

    return e;
}

ByteBuffer codegen_expression(Module* mod, Expression* expr, size_t scope) {
    // TODO add type stack to type check expression
    ByteBuffer out = {0};
    for (size_t i = 0; i < expr->count; i++) {
        ByteBuffer e = codegen_expr(mod, &expr->items[i], scope);
        bb_append_bb(&out, &e);
        free(e.items);
    }
    return out;
}

// statements

ByteBuffer codegen_statement(Module* mod, Statement* st, size_t scope);

ByteBuffer codegen_block_statement(Module* mod, BlockStatement* st,
                                   size_t scope) {
    ByteBuffer block = {0};
    for (size_t i = 0; i < st->count; i++) {
        ByteBuffer child = codegen_statement(mod, &st->items[i], st->scope);
        bb_append_bb(&block, &child);
        free(child.items);
    }
    return block;
}

ByteBuffer codegen_return_statement(Module* mod, ReturnStatement* st,
                                    size_t scope) {
    ByteBuffer ret = codegen_expression(mod, &st->expr, scope);
    da_append(ret, 0x0F);  // return op code
    return ret;
}

ByteBuffer codegen_statement(Module* mod, Statement* st, size_t scope) {
    switch (st->kind) {
        case SK_EMPTY:
            return (ByteBuffer){0};
        case SK_BLOCK:
            return codegen_block_statement(mod, &st->block, scope);
        case SK_RETURN:
            return codegen_return_statement(mod, &st->ret, scope);
    }
}

// functions

Vec codegen_function_locals(Module* mod, Function* f) {
    // TODO this is just a stub, because currently there is no use for locals
    Vec locals = {0};
    return locals;
}

ByteBuffer codegen_function_expr(Module* mod, Function* f) {
    // TODO this is just a stub, because statement codegen is not implemented
    ByteBuffer expr = codegen_statement(mod, &f->content, f->param_scope);
    da_append(expr, 0x0B);  // end opcode
    return expr;
}

ByteBuffer codegen_function(Module* mod, Function* f) {
    ByteBuffer code = {0};

    {
        Vec locals = codegen_function_locals(mod, f);
        bb_append_vec(&code, &locals);
        free(locals.content.items);
    }

    {
        ByteBuffer expr = codegen_function_expr(mod, f);
        bb_append_bb(&code, &expr);
        free(expr.items);
    }

    return code;
}

// sections

Section codegen_types(Module* mod) {
    Section type_section = {.id = SID_TYPE};

    Vec types = {0};

    for (size_t i = 0; i < mod->functions.count; i++) {
        Function* f = &mod->functions.items[i];
        ByteBuffer ft = {0};
        // magic byte
        // https://webassembly.github.io/spec/core/binary/types.html#binary-functype
        da_append(ft, 0x60);

        {  // param types
            DeclScope* param_scope = &mod->scopes.items[f->param_scope];
            Vec param_types = {0};

            for (size_t j = 0; j < param_scope->count; j++) {
                assert(param_scope->items[j].kind == DK_VARIABLE);
                ByteBuffer param_type =
                    codegen_value_type(mod, param_scope->items[j].value);

                vec_append_elem(&param_types, &param_type);
                free(param_type.items);
            }

            bb_append_vec(&ft, &param_types);
            free(param_types.content.items);
        }

        {  // return type
            Vec result_types = {0};

            if (f->return_type !=
                VT_NIL) {  // TODO support multiple return values?
                ByteBuffer result_type =
                    codegen_value_type(mod, f->return_type);

                vec_append_elem(&result_types, &result_type);
                free(result_type.items);
            }

            bb_append_vec(&ft, &result_types);
            free(result_types.content.items);
        }

        vec_append_elem(&types, &ft);
        free(ft.items);
    }

    bb_append_vec(&type_section.content, &types);
    free(types.content.items);

    return type_section;
}

Section codegen_funcs(Module* mod) {
    Section func_section = {.id = SID_FUNCTION};

    Vec funcs = {0};

    for (size_t i = 0; i < mod->functions.count; i++) {
        // TODO
        // this is a very naive way of deducing function types
        ByteBuffer fn = {0};
        bb_append_leb128_u(&fn, i);
        vec_append_elem(&funcs, &fn);
        free(fn.items);
    }

    bb_append_vec(&func_section.content, &funcs);
    free(funcs.content.items);

    return func_section;
}

Section codegen_exports(Module* mod) {
    Section export_section = {.id = SID_EXPORT};

    Vec exports = {0};

    for (size_t i = 0; i < mod->exports.count; i++) {
        Decl* decl = find_global_decl(mod, mod->exports.items[i].decl_name);
        if (decl->kind != DK_FUNCTION) {
            fprintf(stderr, "Unsupported export decl kind %d!\n", decl->kind);
            exit(1);
        }

        ByteBuffer ex = {0};
        bb_append_name(&ex, decl->name);
        da_append(ex, 0x00);  // TODO support other export types
        bb_append_leb128_u(&ex, decl->value);

        vec_append_elem(&exports, &ex);

        free(ex.items);
    }

    bb_append_vec(&export_section.content, &exports);
    free(exports.content.items);

    return export_section;
}

Section codegen_codes(Module* mod) {
    Section code_section = {.id = SID_CODE};

    Vec codes = {0};

    for (size_t i = 0; i < mod->functions.count; i++) {
        ByteBuffer code = {0};
        ByteBuffer func = codegen_function(mod, &mod->functions.items[i]);
        bb_append_leb128_u(&code, func.count);
        bb_append_bb(&code, &func);
        free(func.items);

        vec_append_elem(&codes, &code);
        free(code.items);
    }

    bb_append_vec(&code_section.content, &codes);
    free(codes.content.items);

    return code_section;
}

ByteBuffer codegen_module(Module* mod) {
    Generator gen = {0};
    // magic
    bb_append_bytes(&gen.output_buffer, (uint8_t[]){0x00, 0x61, 0x73, 0x6D}, 4);
    // version
    bb_append_bytes(&gen.output_buffer, (uint8_t[]){0x01, 0x00, 0x00, 0x00}, 4);

    {  // types
        Section type_section = codegen_types(mod);
        bb_append_section(&gen.output_buffer, &type_section);
        free(type_section.content.items);
    }

    {  // funcs
        Section funcs_section = codegen_funcs(mod);
        bb_append_section(&gen.output_buffer, &funcs_section);
        free(funcs_section.content.items);
    }

    {  // exports
        Section export_section = codegen_exports(mod);
        bb_append_section(&gen.output_buffer, &export_section);
        free(export_section.content.items);
    }

    {  // codes
        Section code_section = codegen_codes(mod);
        bb_append_section(&gen.output_buffer, &code_section);
        free(code_section.content.items);
    }

    return gen.output_buffer;
}
