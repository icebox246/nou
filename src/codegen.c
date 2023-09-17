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

typedef enum {
    GLOBAL_STACK_PTR,
} BuiltinGlobals;

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

typedef struct ExprDecision {
    bool take_reference;
    ValueType left_type;
    ValueType right_type;
    size_t dependency;
} ExprDecision;

typedef struct {
    da_list(ExprDecision);
} ExprDecisions;

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
    while (x) {
        uint8_t byte = (x & 0x7F);
        x >>= 7;
        byte |= 0x80;
        da_append(*bb, byte);
    }
    da_append(*bb, 0);
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
    switch (vt.kind) {
        case VT_NIL:
            fprintf(stderr, "Nil value type should not be codegenned\n");
            exit(1);
            break;
        case VT_INT: {
            assert(vt.props.i.bits <= 32);
            ByteBuffer bb = {0};
            da_append(bb, 0x7F);
            return bb;
        } break;
        case VT_BOOL: {  // bool internally gets codegenned as i32
            ByteBuffer bb = {0};
            da_append(bb, 0x7F);
            return bb;
        } break;
        case VT_SLICE: {  // slice is just i64 := {count := i32, ptr := i32}
            ByteBuffer bb = {0};
            da_append(bb, 0x7E);
            return bb;
        } break;
    }

    assert(false && "Unreachable");
}

size_t get_size_of_value_type(Module* mod, ValueType vt) {
    switch (vt.kind) {
        case VT_NIL:
            return 0;
        case VT_INT:
            return (vt.props.i.bits + 7) / 8;
        case VT_BOOL:
            return 1;
            break;
        case VT_SLICE:
            return 8;
            break;
    }
}

// expressions

bool find_local_var(Module* mod, size_t scope, char* name, size_t* out_index,
                    Decl** out_decl) {
    if (out_index) *out_index = 0;
    DeclScope* s = &mod->scopes.items[scope];

    if (scope != 0) {
        if (find_local_var(mod, s->parent, name, out_index, out_decl))
            return true;
    }

    size_t param_index = 0;
    for (size_t i = 0; i < s->count; i++) {
        Decl* decl = &s->items[i];
        if (strcmp(decl->name, name) == 0) {
            if (decl->kind == DK_PARAM && out_index) *out_index = param_index;
            if (out_decl) *out_decl = decl;
            return true;
        }
        if (decl->kind == DK_PARAM) param_index++;
        if (out_index && decl->kind == DK_VARIABLE)
            *out_index += get_size_of_value_type(mod, decl->value.vt);
    }

    return false;
}

bool find_local_fn(Module* mod, size_t scope, char* name, size_t* out_index) {
    DeclScope* s = &mod->scopes.items[scope];

    if (scope != 0) {
        if (find_local_fn(mod, s->parent, name, out_index)) return true;
    }

    for (size_t i = 0; i < s->count; i++) {
        Decl* decl = &s->items[i];
        if (strcmp(decl->name, name) == 0) {
            if (decl->kind == DK_FUNCTION) {
                if (out_index)
                    *out_index =
                        decl->value.func_index + mod->extern_functions.count;
                return true;
            }
            if (decl->kind == DK_EXTERN_FUNCTION) {
                if (out_index) *out_index = decl->value.func_index;
                return true;
            }
            return false;
        }
    }

    return false;
}

bool calc_local_frame_size(Module* mod, size_t scope, size_t* out_size) {
    if (out_size) *out_size = 0;
    DeclScope* s = &mod->scopes.items[scope];

    if (scope == 0) {
        return false;
    }

    if (!s->param_scope) {
        if (!calc_local_frame_size(mod, s->parent, out_size)) return false;
    }

    for (size_t i = 0; i < s->count; i++) {
        Decl* decl = &s->items[i];
        if (decl->kind == DK_VARIABLE) {
            if (out_size)
                *out_size += get_size_of_value_type(mod, decl->value.vt);
        }
    }

    return true;
}

bool find_stack_base_index(Module* mod, size_t scope, size_t* out_index) {
    if (scope == 0) return false;
    const DeclScope* s = &mod->scopes.items[scope];
    if (s->param_scope) {
        if (out_index)
            *out_index = s->count;  // asserts that stack base is the first
                                    // local after params
        return true;
    }
    return find_stack_base_index(mod, s->parent, out_index);
}

bool find_temp_i32_index(Module* mod, size_t scope, size_t* out_index) {
    if (find_stack_base_index(mod, scope, out_index)) {
        if (out_index)
            *out_index += 1;  // asserts that temp_i32 is next after stack_base
        return true;
    }
    return false;
}

bool find_temp_i64_index(Module* mod, size_t scope, size_t* out_index) {
    if (find_stack_base_index(mod, scope, out_index)) {
        if (out_index)
            *out_index += 2;  // asserts that temp_i64 is next after temp_i32
        return true;
    }
    return false;
}

void bb_append_applying_bitmask_i32(ByteBuffer* bb, int bits) {
    if (bits < 32) {
        da_append(*bb, 0x41);  // opcode for i32.const
        bb_append_leb128_u(bb, (1 << bits) - 1);
        da_append(*bb, 0x71);  // opcode for i32.and
    }
}

bool get_string_constant_offset(Module* mod, size_t index, size_t* offset) {
    if (index > mod->string_constants.count) return false;
    if (offset) *offset = 0;
    for (size_t i = 0; i < index; i++) {
        if (offset) *offset += mod->string_constants.items[i].len;
    }
    return true;
}

bool bb_append_loading_value(ByteBuffer* e, Module* mod, ValueType vt,
                             size_t offset) {
    switch (vt.kind) {
        case VT_INT:
            switch (vt.props.i.bits) {
                case 8:
                    da_append(*e,
                              0x2D);           // opcode for i32.load8_u
                    bb_append_leb128_u(e, 0);  // align
                    bb_append_leb128_u(e,
                                       offset);  // offset
                    break;
                case 32:
                    da_append(*e,
                              0x28);           // opcode for i32.load
                    bb_append_leb128_u(e, 0);  // align
                    bb_append_leb128_u(e,
                                       offset);  // offset
                    break;
                default:
                    fprintf(stderr,
                            "%d-bit integers are not "
                            "supported!\n",
                            vt.props.i.bits);
                    return false;
            }
            break;
        case VT_BOOL:
            da_append(*e, 0x2D);            // opcode for i32.load8_u
            bb_append_leb128_u(e, 0);       // align
            bb_append_leb128_u(e, offset);  // offset
            break;
        case VT_SLICE:
            da_append(*e, 0x29);            // opcode for i64.load
            bb_append_leb128_u(e, 0);       // align
            bb_append_leb128_u(e, offset);  // offset
            break;
        default:
            fprintf(stderr, "Unsupported variable type!\n");
            return false;
    }
    return true;
}

ByteBuffer codegen_expr(Module* mod, Expr* ex, ExprDecision decision,
                        ExprDecisions decisions, size_t scope) {
    ByteBuffer e = {0};
    switch (ex->kind) {
        case EK_INT_CONST:
            assert(!decision.take_reference &&
                   "Cannot take reference to a constant");
            switch (ex->props.i.bits) {
                case 8:
                case 32:
                    da_append(e, 0x41);  // opcode for i32.const
                    bb_append_leb128_u(
                        &e,
                        ex->props.i.value);  // TODO make it signed
                    break;
                default:
                    fprintf(stderr, "%d-bit integers are not supported!\n",
                            ex->props.i.bits);
                    assert(false && "Unsupported int size");
            }
            break;
        case EK_BOOL_CONST:
            assert(!decision.take_reference &&
                   "Cannot take reference to a constant");
            da_append(e, 0x41);  // opcode for i32.const
            bb_append_leb128_u(&e, ex->props.boolean);
            break;
        case EK_STRING_CONST: {
            assert(!decision.take_reference &&
                   "Cannot take reference to a constant");
            size_t ptr;
            assert(get_string_constant_offset(mod, ex->props.str_index, &ptr));
            size_t len = mod->string_constants.items[ex->props.str_index].len;
            if (sizeof(size_t) > 4)
                assert(ptr < (1LL << 32) && len < (1LL << 32));
            uint64_t slice = ((uint64_t)len << 32) | (uint64_t)ptr;

            da_append(e, 0x42);  // opcode for i64.const
            bb_append_leb128_u(&e, slice);
        } break;
        case EK_VAR: {
            size_t var_index;
            Decl* var_decl;
            if (!find_local_var(mod, scope, ex->props.var, &var_index,
                                &var_decl)) {
                fprintf(stderr, "Can't find `%s` used in expr\n",
                        ex->props.var);
                exit(1);
            }
            switch (var_decl->kind) {
                case DK_FUNCTION:
                    assert(false && "Unimplemented");
                    break;
                case DK_EXTERN_FUNCTION:
                    assert(false && "Unimplemented");
                    break;
                case DK_PARAM:
                    assert(!decision.take_reference &&
                           "Cannot take reference to a parameter");
                    da_append(e, 0x20);  // opcode for local.get
                    bb_append_leb128_u(&e, var_index);
                    break;
                case DK_VARIABLE: {
                    size_t stack_base_index;
                    if (!find_stack_base_index(mod, scope, &stack_base_index)) {
                        assert(false &&
                               "Failed to find stack_base, likely expression "
                               "is not in a function scope");
                    }
                    if (decision.take_reference) {
                        da_append(e, 0x20);  // opcode for local.get
                        bb_append_leb128_u(&e, stack_base_index);

                        if (var_index) {
                            da_append(e, 0x41);  // opcode for i32.const
                            bb_append_leb128_u(&e, var_index);  // offset
                            da_append(e, 0x6A);  // opcode for i32.add
                        }
                    } else {
                        da_append(e, 0x20);  // opcode for local.get
                        bb_append_leb128_u(&e, stack_base_index);

                        assert(bb_append_loading_value(
                            &e, mod, var_decl->value.vt, var_index));
                    }
                } break;
            }
        } break;
        case EK_OPERATOR: {
            switch (ex->props.op) {
                case OP_ADDITION:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x6A);  // opcode for i32.add
                    bb_append_applying_bitmask_i32(
                        &e, decision.left_type.props.i.bits);
                    break;
                case OP_SUBTRACTION:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x6B);  // opcode for i32.sub
                    bb_append_applying_bitmask_i32(
                        &e, decision.left_type.props.i.bits);
                    break;
                case OP_MULTIPLICATION:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x6C);  // opcode for i32.mul
                    bb_append_applying_bitmask_i32(
                        &e, decision.left_type.props.i.bits);
                    break;
                case OP_REMAINDER:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    if (!decision.left_type.props.i.unsign) {
                        da_append(e, 0x6F);  // opcode for i32.rem_s
                    } else {
                        da_append(e, 0x70);  // opcode for i32.rem_u
                    }
                    bb_append_applying_bitmask_i32(
                        &e, decision.left_type.props.i.bits);
                    break;
                case OP_DIVISION:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    if (!decision.left_type.props.i.unsign) {
                        da_append(e, 0x6D);  // opcode for i32.div_s
                    } else {
                        da_append(e, 0x6E);  // opcode for i32.div_u
                    }
                    bb_append_applying_bitmask_i32(
                        &e, decision.left_type.props.i.bits);
                    break;
                case OP_EQUALITY:
                    assert(decision.left_type.kind == VT_INT &&
                           compare_value_types(decision.left_type,
                                               decision.right_type));
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x46);  // opcode for i32.eq
                    break;
                case OP_ALTERNATIVE:
                    assert(decision.left_type.kind == VT_BOOL &&
                           decision.right_type.kind == VT_BOOL);
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x72);  // opcode for i32.or
                    break;
                case OP_CONJUNCTION:
                    assert(decision.left_type.kind == VT_BOOL &&
                           decision.right_type.kind == VT_BOOL);
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");
                    da_append(e, 0x71);  // opcode for i32.and
                    break;
                case OP_INDEXING: {
                    assert(decision.left_type.kind == VT_SLICE &&
                           decision.right_type.kind == VT_INT);

                    ValueType item_type = *decision.left_type.props.inner_type;

                    size_t temp_i32_index;
                    assert(find_temp_i32_index(mod, scope, &temp_i32_index));

                    da_append(e, 0x21);  // opcode for local.set
                    bb_append_leb128_u(&e, temp_i32_index);

                    da_append(e, 0xA7);  // opcode for i32.wrap_i64

                    da_append(e, 0x20);  // opcode for local.get
                    bb_append_leb128_u(&e, temp_i32_index);

                    size_t size_of_item =
                        get_size_of_value_type(mod, item_type);

                    if (size_of_item != 1) {
                        da_append(e, 0x41);  // opcode for i32.const
                        bb_append_leb128_u(&e, size_of_item);
                        da_append(e, 0x6C);  // opcode for i32.mul
                    }

                    da_append(e, 0x6A);  // opcode for i32.add

                    if (!decision.take_reference)
                        assert(bb_append_loading_value(&e, mod, item_type, 0));
                } break;
                case OP_ASSIGNEMENT: {
                    if (!compare_value_types(decision.left_type,
                                             decision.right_type)) {
                        assert(false && "Non matching types in assignement");
                    }

                    assert(decision.left_type.kind != VT_INT ||
                           decision.left_type.props.i.bits <= 32);
                    assert(!decision.take_reference &&
                           "Cannot take reference to a temporary");

                    size_t temp_i32_index;
                    assert(find_temp_i32_index(mod, scope, &temp_i32_index));

                    size_t temp_i64_index;
                    assert(find_temp_i64_index(mod, scope, &temp_i64_index));

                    switch (decision.left_type.kind) {
                        case VT_NIL:
                            assert(false && "Cannot assing to nil value type");
                            break;
                        case VT_INT: {
                            da_append(e, 0x22);  // opcode for local.tee
                            bb_append_leb128_u(&e, temp_i32_index);

                            switch (decision.left_type.props.i.bits) {
                                case 8:
                                    da_append(e,
                                              0x3A);  // opcode for i32.store8
                                    bb_append_leb128_u(&e, 0);
                                    bb_append_leb128_u(&e, 0);
                                    break;
                                case 32:
                                    da_append(e, 0x36);  // opcode for i32.store
                                    bb_append_leb128_u(&e, 0);
                                    bb_append_leb128_u(&e, 0);
                                    break;
                                default:
                                    fprintf(
                                        stderr,
                                        "%d-bit integers are not supported!\n",
                                        decision.left_type.props.i.bits);
                                    assert(false && "Unsupported int size");
                            }

                            da_append(e, 0x20);  // opcode for local.get
                            bb_append_leb128_u(&e, temp_i32_index);
                        } break;
                        case VT_BOOL: {
                            da_append(e, 0x22);  // opcode for local.tee
                            bb_append_leb128_u(&e, temp_i32_index);

                            da_append(e, 0x3A);  // opcode for i32.store8
                            bb_append_leb128_u(&e, 0);
                            bb_append_leb128_u(&e, 0);

                            da_append(e, 0x20);  // opcode for local.get
                            bb_append_leb128_u(&e, temp_i32_index);
                        } break;
                        case VT_SLICE: {
                            da_append(e, 0x22);  // opcode for local.tee
                            bb_append_leb128_u(&e, temp_i64_index);

                            da_append(e, 0x37);  // opcode for i64.store
                            bb_append_leb128_u(&e, 0);
                            bb_append_leb128_u(&e, 0);

                            da_append(e, 0x20);  // opcode for local.get
                            bb_append_leb128_u(&e, temp_i64_index);
                        } break;
                    }
                } break;

                case OP_OPEN_PAREN:
                case OP_FUNC_CALL:
                case OP_FIELD_ACCESS:
                    assert(false && "Unreachable");
            }
        } break;
        case EK_FUNC_CALL: {
            assert(!decision.take_reference &&
                   "Cannot take reference to a temporary");

            size_t stack_base_index;
            assert(find_stack_base_index(mod, scope, &stack_base_index));

            size_t frame_size;
            assert(calc_local_frame_size(mod, scope, &frame_size));

            size_t fn_index;
            assert(find_local_fn(mod, scope, ex->props.func, &fn_index));

            da_append(e, 0x20);  // opcode for local.get
            bb_append_leb128_u(&e, stack_base_index);
            da_append(e, 0x41);  // opcode for i32.const
            bb_append_leb128_u(&e, frame_size);
            da_append(e, 0x6A);  // opcode for i32.add
            da_append(e, 0x24);  // opcode for global.set
            bb_append_leb128_u(&e, GLOBAL_STACK_PTR);

            da_append(e, 0x10);  // opcode for call
            bb_append_leb128_u(&e, fn_index);
        } break;
        case EK_FIELD_ACCESS: {
            if (decision.left_type.kind == VT_SLICE) {
                if (decision.take_reference) {
                    assert(decision.dependency != -1);
                    assert(decisions.items[decision.dependency].take_reference);
                    if (strcmp(ex->props.field_name, "len") == 0) {
                        da_append(e, 0x41);  // opcode for i32.const
                        bb_append_leb128_u(
                            &e,
                            4);  // offset of .len from right (bytes)
                        da_append(e, 0x6A);  // opcode for i32.add
                    } else if (strcmp(ex->props.field_name, "ptr") == 0) {
                        // nop
                    } else {
                        assert(false && "Invalid field on slice");
                    }
                } else {
                    if (strcmp(ex->props.field_name, "len") == 0) {
                        da_append(e, 0x42);  // opcode for i64.const
                        bb_append_leb128_u(
                            &e,
                            32);             // offset of .len from right (bits)
                        da_append(e, 0x88);  // opcode for i64.shr_u
                        da_append(e, 0xA7);  // opcode for i32.wrap_i64
                    } else if (strcmp(ex->props.field_name, "ptr") == 0) {
                        da_append(e, 0xA7);  // opcode for i32.wrap_i64
                    } else {
                        assert(false && "Invalid field on slice");
                    }
                }
            } else {
                assert("Unimplemented");
            }
        } break;
    }

    return e;
}

ExprDecisions compute_expression_decisions(Module* mod, Expression* expr,
                                           size_t scope,
                                           ValueType* out_remaining_value) {
    struct {
        da_list(size_t);
    } index_stack = {0};
    struct {
        da_list(ValueType);
    } type_stack = {0};
    ExprDecisions decisions = {0};

    for (size_t i = 0; i < expr->count; i++) {
        ExprDecision decision = {.dependency = -1};
        Expr* e = &expr->items[i];

        switch (e->kind) {
            case EK_INT_CONST: {
                da_append(index_stack, i);
                ValueType vt = {
                    .kind = VT_INT,
                    .props.i.bits = e->props.i.bits,
                    .props.i.unsign = e->props.i.unsign,
                };
                da_append(type_stack, vt);
            } break;
            case EK_BOOL_CONST: {
                da_append(index_stack, i);
                da_append(type_stack, (ValueType){.kind = VT_BOOL});
            } break;
            case EK_STRING_CONST: {
                da_append(index_stack, i);

                ValueType vt = {
                    .kind = VT_SLICE,
                };
                vt.props.inner_type = malloc(sizeof(ValueType));
                assert(vt.props.inner_type);

                *vt.props.inner_type = (ValueType){
                    .kind = VT_INT,
                    .props.i.bits = 8,
                    .props.i.unsign = true,
                };

                da_append(type_stack, vt);
            } break;
            case EK_VAR: {
                Decl* decl;
                assert(find_local_var(mod, scope, e->props.var, NULL, &decl) &&
                       "Use of undeclared variable");
                da_append(index_stack, i);
                da_append(type_stack, decl->value.vt);
            } break;
            case EK_OPERATOR: {
                assert(index_stack.count >= 2 &&
                       "Wrong amount of operands for binary op");
                size_t li = index_stack.items[index_stack.count - 2];
                size_t ri = index_stack.items[index_stack.count - 1];
                decision.left_type = type_stack.items[index_stack.count - 2];
                decision.right_type = type_stack.items[index_stack.count - 1];
                switch (e->props.op) {
                    case OP_ADDITION:
                    case OP_SUBTRACTION:
                    case OP_MULTIPLICATION:
                    case OP_REMAINDER:
                    case OP_DIVISION:
                    case OP_ALTERNATIVE:
                    case OP_CONJUNCTION: {
                        index_stack.count -= 2;
                        type_stack.count -= 2;

                        da_append(index_stack, i);
                        da_append(
                            type_stack,
                            decision.left_type);  // TODO maybe there should be
                                                  // a smarter system for that
                    } break;
                    case OP_INDEXING: {
                        index_stack.count -= 2;
                        type_stack.count -= 2;

                        assert(decision.left_type.kind == VT_SLICE &&
                               "Cannot index non slice values");

                        da_append(index_stack, i);
                        da_append(type_stack,
                                  *decision.left_type.props.inner_type);
                    } break;
                    case OP_EQUALITY: {
                        index_stack.count -= 2;
                        type_stack.count -= 2;
                        da_append(index_stack, i);
                        da_append(type_stack, (ValueType){.kind = VT_BOOL});
                    } break;
                    case OP_ASSIGNEMENT: {
                        index_stack.count -= 2;
                        type_stack.count -= 2;
                        {
                            size_t it = li;
                            do {
                                decisions.items[it].take_reference = true;
                            } while ((it = decisions.items[it].dependency) !=
                                     -1);
                        }

                        da_append(index_stack, i);
                        da_append(type_stack, decision.left_type);
                    } break;

                    case OP_FIELD_ACCESS:
                        assert(false && "Unimplemented");
                        break;

                    case OP_OPEN_PAREN:
                    case OP_FUNC_CALL:
                        assert(false && "Unreachable");
                        break;
                }
            } break;
            case EK_FUNC_CALL: {
                size_t fn_index;
                assert(find_local_fn(mod, scope, e->props.func, &fn_index) &&
                       "Calling undefined function");
                Function* f;
                if (fn_index >= mod->extern_functions.count)
                    f = &mod->functions
                             .items[fn_index - mod->extern_functions.count];
                else
                    f = &mod->extern_functions.items[fn_index];

                DeclScope* param_scope = &mod->scopes.items[f->param_scope];
                size_t arity = param_scope->count;

                assert(type_stack.count >= arity);

                for (int i = 0; i < arity; i++) {
                    if (!compare_value_types(
                            type_stack.items[type_stack.count - arity + i],
                            param_scope->items[i].value.vt))
                        assert(false && "Type mismatch in function arguments");
                }

                index_stack.count -= arity;
                type_stack.count -= arity;

                if (f->return_type.kind != VT_NIL) {
                    da_append(index_stack, i);
                    da_append(type_stack, f->return_type);
                }
            } break;
            case EK_FIELD_ACCESS: {
                assert(type_stack.count > 0 &&
                       "There has to be value of which a field is accessed");
                ValueType object_type = type_stack.items[type_stack.count - 1];

                if (object_type.kind != VT_SLICE)
                    assert(false && "Unimplemented");

                if (strcmp(e->props.field_name, "len") != 0 &&
                    strcmp(e->props.field_name, "ptr") != 0)
                    assert(false && "Unimplemented");

                ValueType vt = {
                    // u32 type
                    .kind = VT_INT,
                    .props.i.bits = 32,
                    .props.i.unsign = true,
                };
                decision.dependency = index_stack.items[index_stack.count - 1];

                decision.left_type = object_type;
                decision.right_type = vt;

                // remove parent object from stack
                index_stack.count--;
                type_stack.count--;

                // push field object on stack
                da_append(index_stack, i);
                da_append(type_stack, vt);
            } break;
        }
        da_append(decisions, decision);
    }

    if (out_remaining_value) {
        switch (type_stack.count) {
            case 1:
                *out_remaining_value = *type_stack.items;
                break;
            case 0:
                out_remaining_value->kind = VT_NIL;
                break;
            default:
                assert(false &&
                       "There must be a single remaining value after "
                       "processing an expression");
        }
    }

    free(index_stack.items);
    free(type_stack.items);
    return decisions;
}

ByteBuffer codegen_expression(Module* mod, Expression* expr, size_t scope,
                              ValueType* out_remaining_value) {
    ExprDecisions decisions =
        compute_expression_decisions(mod, expr, scope, out_remaining_value);
    ByteBuffer out = {0};
    for (size_t i = 0; i < expr->count; i++) {
        ByteBuffer e = codegen_expr(mod, &expr->items[i], decisions.items[i],
                                    decisions, scope);
        bb_append_bb(&out, &e);
        free(e.items);
    }
    free(decisions.items);
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
    // TODO make sure correct value gets returned
    ByteBuffer ret = codegen_expression(mod, &st->expr, scope, NULL);
    da_append(ret, 0x0F);  // opcode for return
    return ret;
}

ByteBuffer codegen_if_statement(Module* mod, IfStatement* st, size_t scope) {
    ValueType cond_vt;
    ByteBuffer ifs = codegen_expression(mod, &st->cond_expr, scope, &cond_vt);
    assert(cond_vt.kind == VT_BOOL &&
           "Condition of if statement must be a boolean");
    da_append(ifs, 0x04);  // opcode for if
    da_append(ifs, 0x40);  // opcode for nil result type
    {
        ByteBuffer positive_branch =
            codegen_statement(mod, st->positive_branch, scope);
        bb_append_bb(&ifs, &positive_branch);
        free(positive_branch.items);
    }
    if (st->negative_branch) {  // has else clause
        da_append(ifs, 0x05);   // opcode for else
        ByteBuffer negative_branch =
            codegen_statement(mod, st->negative_branch, scope);
        bb_append_bb(&ifs, &negative_branch);
        free(negative_branch.items);
    }
    da_append(ifs, 0x0B);  // opcode for end
    return ifs;
}

ByteBuffer codegen_expr_statement(Module* mod, ExpressionStatement* st,
                                  size_t scope) {
    ValueType drop_value;
    ByteBuffer ex = codegen_expression(mod, &st->expr, scope, &drop_value);
    if (drop_value.kind != VT_NIL) {
        da_append(ex, 0x1A);  // opcode for drop
    }
    return ex;
}

ByteBuffer codegen_statement(Module* mod, Statement* st, size_t scope) {
    switch (st->kind) {
        case SK_EMPTY:
            return (ByteBuffer){0};
        case SK_BLOCK:
            return codegen_block_statement(mod, &st->block, scope);
        case SK_RETURN:
            return codegen_return_statement(mod, &st->ret, scope);
        case SK_IF:
            return codegen_if_statement(mod, &st->ifs, scope);
        case SK_EXPRESSION:
            return codegen_expr_statement(mod, &st->expr, scope);
    }
}

// functions

Vec codegen_function_locals(Module* mod, Function* f) {
    Vec locals = {0};
    {  // stack_base and temp_i32
        ByteBuffer stack_base_local_buf = {0};
        bb_append_leb128_u(&stack_base_local_buf, 2);
        da_append(stack_base_local_buf, 0x7F);  // i32
        vec_append_elem(&locals, &stack_base_local_buf);
        free(stack_base_local_buf.items);
    }

    {  // temp_i64
        ByteBuffer temp_i64_local_buf = {0};
        bb_append_leb128_u(&temp_i64_local_buf, 1);
        da_append(temp_i64_local_buf, 0x7E);  // i64
        vec_append_elem(&locals, &temp_i64_local_buf);
        free(temp_i64_local_buf.items);
    }
    return locals;
}

ByteBuffer codegen_function_expr(Module* mod, Function* f) {
    ByteBuffer expr = codegen_statement(mod, &f->content, f->param_scope);

    if (f->return_type.kind != VT_NIL)  // disable implicit return
        da_append(expr, 0x00);          // opcode for unreachable

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
        size_t stack_base_index;
        assert(find_stack_base_index(mod, f->param_scope, &stack_base_index) &&
               "Function param scope must be a param_scope...");

        da_append(code, 0x23);  // opcode for global.get
        bb_append_leb128_u(&code, GLOBAL_STACK_PTR);

        da_append(code, 0x21);  // opcode for local.set
        bb_append_leb128_u(&code, stack_base_index);
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

    for (size_t i = 0; i < mod->function_types.count; i++) {
        FunctionType* f = &mod->function_types.items[i];
        ByteBuffer ft = {0};
        // magic byte
        // https://webassembly.github.io/spec/core/binary/types.html#binary-functype
        da_append(ft, 0x60);

        {  // param types
            DeclScope* param_scope = &mod->scopes.items[f->param_scope];
            Vec param_types = {0};

            for (size_t j = 0; j < param_scope->count; j++) {
                assert(param_scope->items[j].kind == DK_PARAM);
                ByteBuffer param_type =
                    codegen_value_type(mod, param_scope->items[j].value.vt);

                vec_append_elem(&param_types, &param_type);
                free(param_type.items);
            }

            bb_append_vec(&ft, &param_types);
            free(param_types.content.items);
        }

        {  // return type
            Vec result_types = {0};

            if (f->return_type.kind !=
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
        ByteBuffer fn = {0};
        bb_append_leb128_u(&fn, mod->functions.items[i].function_type);
        vec_append_elem(&funcs, &fn);
        free(fn.items);
    }

    bb_append_vec(&func_section.content, &funcs);
    free(funcs.content.items);

    return func_section;
}

Section codegen_import(Module* mod) {
    Section import_section = {.id = SID_IMPORT};

    Vec imports = {0};

    for (size_t i = 0; i < mod->extern_functions.count; i++) {
        ByteBuffer imp = {0};
        // TODO make module name customizable
        bb_append_name(&imp, "env");  // module
        char* name = NULL;
        for (size_t j = 0; j < mod->scopes.items[0].count; j++) {
            Decl* d = &mod->scopes.items[0].items[j];
            if (d->kind == DK_EXTERN_FUNCTION && d->value.func_index == i) {
                name = d->name;
            }
        }
        if (name == NULL) {
            assert(false && "Could not find decl of extern function");
        }
        bb_append_name(&imp, name);  // name
        da_append(imp, 0x00);        // func
        bb_append_leb128_u(&imp, mod->extern_functions.items[i].function_type);
        vec_append_elem(&imports, &imp);
        free(imp.items);
    }

    bb_append_vec(&import_section.content, &imports);
    free(imports.content.items);

    return import_section;
}

Section codegen_mem(Module* mod) {
    Section mem_section = {.id = SID_MEMORY};

    Vec mems = {0};

    {
        ByteBuffer memory0 = {0};
        da_append(memory0, 0x00);  // limit: min..
        bb_append_leb128_u(&memory0, 2);
        vec_append_elem(&mems, &memory0);
        free(memory0.items);
    }

    bb_append_vec(&mem_section.content, &mems);
    free(mems.content.items);

    return mem_section;
}

Section codegen_global(Module* mod) {
    Section global_section = {.id = SID_GLOBAL};

    Vec globals = {0};

    size_t constants_size = 0;
    for (size_t i = 0; i < mod->string_constants.count; i++) {
        constants_size += mod->string_constants.items[i].len;
    }

    {
        ByteBuffer stack_ptr = {0};
        da_append(stack_ptr, 0x7F);  // i32
        da_append(stack_ptr, 0x01);  // mut
        da_append(stack_ptr, 0x41);  // opcode for i32.const
        bb_append_leb128_u(
            &stack_ptr,
            constants_size);        // start execution stack after constants
        da_append(stack_ptr, 0xB);  // opcode for end
        vec_append_elem(&globals, &stack_ptr);
        free(stack_ptr.items);
    }

    bb_append_vec(&global_section.content, &globals);
    free(globals.content.items);

    return global_section;
}

Section codegen_exports(Module* mod) {
    Section export_section = {.id = SID_EXPORT};

    Vec exports = {0};

    {  // memory export
        ByteBuffer bb = {0};
        bb_append_name(&bb, "u_memory");
        da_append(bb, 0x02);
        bb_append_leb128_u(&bb, 0);
        vec_append_elem(&exports, &bb);
        free(bb.items);
    }

    for (size_t i = 0; i < mod->exports.count; i++) {
        Decl* decl = find_global_decl(mod, mod->exports.items[i].decl_name);
        assert(decl && "Cannot export undeclared function");
        if (decl->kind != DK_FUNCTION) {
            fprintf(stderr, "Unsupported export decl kind %d!\n", decl->kind);
            exit(1);
        }

        ByteBuffer ex = {0};
        bb_append_name(&ex, decl->name);
        da_append(ex, 0x00);  // TODO support other export types
        bb_append_leb128_u(
            &ex, decl->value.func_index + mod->extern_functions.count);

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

Section codegen_datas(Module* mod) {
    Section code_section = {.id = SID_DATA};

    Vec datas = {0};

    {  // string constant data
        ByteBuffer bb = {0};
        bb_append_leb128_u(&bb, 0);  // active data in memory 0
        da_append(bb, 0x41);         // opcode for i32.const
        bb_append_leb128_u(&bb, 0);  // string constants are stored at offset 0
        da_append(bb, 0x0B);         // opcode for end

        {
            Vec bytes = {0};

            ByteBuffer byte = {0};

            for (size_t i = 0; i < mod->string_constants.count; i++) {
                StringConstant s = mod->string_constants.items[i];
                for (size_t j = 0; j < s.len; j++) {
                    da_append(byte, s.chars[j]);
                    vec_append_elem(&bytes, &byte);
                    byte.count = 0;
                }
            }

            free(byte.items);
            bb_append_vec(&bb, &bytes);
        }

        vec_append_elem(&datas, &bb);
        free(bb.items);
    }

    bb_append_vec(&code_section.content, &datas);
    free(datas.content.items);

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

    {  // import
        Section import_section = codegen_import(mod);
        bb_append_section(&gen.output_buffer, &import_section);
        free(import_section.content.items);
    }

    {  // funcs
        Section funcs_section = codegen_funcs(mod);
        bb_append_section(&gen.output_buffer, &funcs_section);
        free(funcs_section.content.items);
    }

    {  // mem
        Section mem_section = codegen_mem(mod);
        bb_append_section(&gen.output_buffer, &mem_section);
        free(mem_section.content.items);
    }

    {  // global
        Section global_section = codegen_global(mod);
        bb_append_section(&gen.output_buffer, &global_section);
        free(global_section.content.items);
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

    {  // datas
        Section data_section = codegen_datas(mod);
        bb_append_section(&gen.output_buffer, &data_section);
        free(data_section.content.items);
    }

    return gen.output_buffer;
}
