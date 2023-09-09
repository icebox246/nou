#include "parse.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool parse_statement(Parser* p, Statement* st);

bool parse_value_type(Parser* p, ValueType* vt) {
    Token token = lexer_next_token(p->lex);
    switch (token) {
        case KW_i32:
            *vt = VT_I32;
            break;
        default:
            fprintf(stderr, "Unexpected token in value type %d!\n", token);
            return false;
    }
    return true;
}

bool check_decl_name_available(Parser* p, char* decl_name) {
    size_t scope = p->current_scope;
    while (true) {
        DeclScope* s = &p->mod->scopes.items[scope];

        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->items[i].name, decl_name) == 0) {
                return false;
            }
        }

        if (scope == 0) return true;
        scope = s->parent;
    }
}

bool parse_function_type(Parser* p, Function* f) {
    assert(p->lex->token == KW_FN);

    f->param_scope = p->mod->scopes.count;
    {
        DeclScope scope = {
            .parent = p->current_scope,
            .param_scope = true,
        };
        da_append(p->mod->scopes, scope);
    }

    Token token;
    while ((token = lexer_next_token(p->lex)) != T_ARROW &&
           token != T_OPEN_BRACKETS) {
        if (token != T_IDENT) {
            fprintf(stderr, "Expected param name identifier, got %d!\n", token);
            return false;
        }
        Decl param = {0};
        param.name = strndup(p->lex->token_text, p->lex->token_len);
        if (!check_decl_name_available(p, param.name)) {
            fprintf(stderr, "Redeclaration of `%s` in param!\n", param.name);
            return false;
        }
        param.kind = DK_PARAM;

        token = lexer_next_token(p->lex);

        if (token != T_COLON) {
            fprintf(stderr, "Expected colon, got %d!\n", token);
            return false;
        }

        if (!parse_value_type(p, (ValueType*)&param.value)) {
            fprintf(stderr, "Failed to parse param type!\n");
            return false;
        }

        da_append(p->mod->scopes.items[f->param_scope], param);

        token = lexer_next_token(p->lex);

        if (token != T_COMMA) {
            if (token == T_ARROW || token == T_OPEN_BRACKETS) break;

            fprintf(stderr, "Expected comma, arrow or '{', got %d!\n", token);
            return false;
        }
    }

    if (token == T_ARROW) {
        if (!parse_value_type(p, &f->return_type)) {
            fprintf(stderr, "Failed to parse return type!\n");
            return false;
        }
    }

    if (p->lex->token == T_OPEN_BRACKETS) {
        lexer_undo_token(p->lex);
    }

    return true;
}

size_t operator_precedence(OperatorKind op) {
    switch (op) {
        case OP_MULTIPLICATION:
            return 3;
        case OP_ADDITION:
        case OP_SUBTRACTION:
            return 2;
        case OP_ASSIGNEMENT:
            return 1;

        case OP_OPEN_PAREN:
        case OP_FUNC_CALL:
            assert(false && "Unreachable");
    }
}

typedef enum {
    OPA_LEFT,
    OPA_RIGHT,
} OpAssociativity;

size_t operator_associativity(OperatorKind op) {
    switch (op) {
        case OP_ADDITION:
        case OP_SUBTRACTION:
        case OP_MULTIPLICATION:
            return OPA_LEFT;

        case OP_ASSIGNEMENT:
            return OPA_RIGHT;

        case OP_OPEN_PAREN:
        case OP_FUNC_CALL:
            assert(false && "Unreachable");
    }
}

OperatorKind operator_of_token(Token tok) {
    switch (tok) {
        case T_ASSIGN:
            return OP_ASSIGNEMENT;
        case T_MINUS:
            return OP_SUBTRACTION;
        case T_PLUS:
            return OP_ADDITION;
        case T_STAR:
            return OP_MULTIPLICATION;
        default:
            return -1;
    }
}

bool parse_expression(Parser* p, Expression* ex, Token terminator) {
    struct {
        da_list(OperatorKind);
    } op_stack = {0};
    struct {
        da_list(char*);
    } func_name_stack = {0};

    while (true) {
        Token token = lexer_next_token(p->lex);
        if (token == terminator) {
            lexer_undo_token(p->lex);
            while (op_stack.count) {
                assert(op_stack.items[op_stack.count - 1] != OP_OPEN_PAREN);
                Expr e = {
                    .kind = EK_OPERATOR,
                    .props.op = op_stack.items[op_stack.count - 1],
                };
                da_append(*ex, e);
                op_stack.count--;
            }
            free(op_stack.items);
            free(func_name_stack.items);
            return true;
        }
        {
            OperatorKind new_op = operator_of_token(token);
            if (new_op != -1) {
                OperatorKind top_op;
                while (op_stack.count &&
                       (top_op = op_stack.items[op_stack.count - 1]) !=
                           OP_OPEN_PAREN &&
                       (operator_precedence(top_op) >
                            operator_precedence(new_op) ||
                        (operator_precedence(top_op) ==
                             operator_precedence(new_op) &&
                         operator_associativity(new_op) == OPA_LEFT))) {
                    Expr e = {
                        .kind = EK_OPERATOR,
                        .props.op = op_stack.items[op_stack.count - 1],
                    };
                    da_append(*ex, e);
                    op_stack.count--;
                }
                da_append(op_stack, new_op);
                continue;
            }
        }
        switch (token) {
            case T_OPEN_PARENS: {
                da_append(op_stack, OP_OPEN_PAREN);
            } break;
            case T_CLOSE_PARENS: {
                if (op_stack.count == 0) {
                    fprintf(stderr, "Mismatched parenthesis!\n");
                    return false;
                }
                while (op_stack.items[op_stack.count - 1] != OP_OPEN_PAREN) {
                    if (op_stack.count == 0) {
                        fprintf(stderr, "Mismatched parenthesis!\n");
                        return false;
                    }
                    Expr e = {
                        .kind = EK_OPERATOR,
                        .props.op = op_stack.items[op_stack.count - 1],
                    };
                    da_append(*ex, e);
                    op_stack.count--;
                }

                assert(op_stack.items[op_stack.count - 1] == OP_OPEN_PAREN);
                op_stack.count--;

                if (op_stack.count &&
                    op_stack.items[op_stack.count - 1] == OP_FUNC_CALL) {
                    Expr e = {
                        .kind = EK_FUNC_CALL,
                        .props.var =
                            func_name_stack.items[func_name_stack.count - 1],
                    };
                    da_append(*ex, e);

                    op_stack.count--;
                    func_name_stack.count--;
                }
            } break;
            case T_IDENT: {
                char* name = strndup(p->lex->token_text, p->lex->token_len);
                if (lexer_next_token(p->lex) == T_OPEN_PARENS) {
                    lexer_undo_token(p->lex);
                    da_append(func_name_stack, name);
                    da_append(op_stack, OP_FUNC_CALL);
                } else {
                    lexer_undo_token(p->lex);
                    Expr e = {
                        .kind = EK_VAR,
                        .props.var = name,
                    };
                    da_append(*ex, e);
                }
            } break;
            case T_INT: {  // TODO should all int consts be treated as i32?
                Expr e = {
                    .kind = EK_I32_CONST,
                    .props.i32 = p->lex->token_int,
                };
                da_append(*ex, e);
            } break;
            case T_COMMA: {
                while (op_stack.count &&
                       op_stack.items[op_stack.count - 1] != OP_OPEN_PAREN) {
                    Expr e = {
                        .kind = EK_OPERATOR,
                        .props.op = op_stack.items[op_stack.count - 1],
                    };
                    fprintf(stderr, "FOO: %d\n",
                            op_stack.items[op_stack.count - 1]);
                    da_append(*ex, e);
                    op_stack.count--;
                }
            } break;
            default:
                fprintf(stderr, "Unexpected token in expression %d: `%.*s`!\n",
                        token, (int)p->lex->token_len, p->lex->token_text);
                return false;
        }
    }

    assert(false && "Unreachable");
}

bool parse_decl_statement(Parser* p, ExpressionStatement* st, char* decl_name) {
    if (!check_decl_name_available(p, decl_name)) {
        fprintf(stderr, "Redeclaration of `%s`!\n", decl_name);
        return false;
    }
    da_append(p->mod->scopes.items[p->current_scope], (Decl){0});
    Decl* d = &p->mod->scopes.items[p->current_scope]
                   .items[p->mod->scopes.items[p->current_scope].count - 1];
    d->name = decl_name;

    Token token = lexer_next_token(p->lex);

    if (token != T_DECLARE) {
        fprintf(stderr, "Expected declare operator, got %d!\n", token);
        return false;
    }

    token = lexer_next_token(p->lex);
    switch (token) {
        case KW_FN: {
            d->kind = DK_FUNCTION;
            Function f = {0};
            size_t old_scope = p->current_scope;
            if (!parse_function_type(p, &f)) {
                fprintf(stderr, "Failed to parse function type!\n");
                return false;
            }
            p->current_scope = f.param_scope;
            if (!parse_statement(p, &f.content)) {
                fprintf(stderr, "Failed to parse function content!\n");
                return false;
            }
            p->current_scope = old_scope;

            d->value = p->mod->functions.count;
            da_append(p->mod->functions, f);
        } break;
        default: {  // try to parse variable type
            lexer_undo_token(p->lex);
            d->kind = DK_VARIABLE;
            if (!parse_value_type(p, (ValueType*)&d->value)) {
                fprintf(stderr, "Failed to parse variable type!\n");
                return false;
            }
            if (st) {  // initialization expression
                st->kind = SK_EXPRESSION;
                {
                    Expr e = {
                        .kind = EK_VAR,
                        .props.var = decl_name,
                    };
                    da_append(st->expr, e);
                }
                if (!parse_expression(p, &st->expr, T_SEMICOLON)) {
                    fprintf(stderr,
                            "Failed to parse initialization expression!\n");
                    return false;
                }
                if (st->expr.count > 1) {
                    Expr e = {
                        .kind = EK_OPERATOR,
                        .props.op = OP_ASSIGNEMENT,
                    };
                    da_append(st->expr, e);
                } else {
                    free(st->expr.items);
                    st->expr = (Expression){0};
                }
            }
        }
    }

    token = lexer_next_token(p->lex);
    if (token != T_SEMICOLON) {
        fprintf(stderr, "Expected semicolon after decl, got %d!\n", token);
        return false;
    }

    return true;
}

bool parse_export_statement(Parser* p) {
    assert(p->lex->token == KW_EXPORT);

    Token token = lexer_next_token(p->lex);

    switch (token) {
        case T_IDENT: {
            Export ex = {0};
            ex.decl_name = strndup(p->lex->token_text, p->lex->token_len);
            da_append(p->mod->exports, ex);
        } break;
        default:
            fprintf(stderr, "Unexpected token export statement %d!\n", token);
            return false;
    }

    token = lexer_next_token(p->lex);

    if (token != T_SEMICOLON) {
        fprintf(stderr, "Expected semicolon after export statement %d!\n",
                token);
        return false;
    }

    return true;
}

bool parse_block_statement(Parser* p, BlockStatement* st) {
    st->kind = SK_BLOCK;
    size_t old_scope = p->current_scope;

    da_append(p->mod->scopes, (DeclScope){.parent = p->current_scope});
    p->current_scope = p->mod->scopes.count - 1;
    st->scope = p->current_scope;

    Token token;
    while ((token = lexer_next_token(p->lex)) != T_CLOSE_BRACKETS) {
        lexer_undo_token(p->lex);
        da_append(*st, (Statement){0});
        if (!parse_statement(p, &st->items[st->count - 1])) {
            fprintf(stderr, "Failed to parse statement in block!\n");
            return false;
        }
    }

    p->current_scope = old_scope;

    return true;
}

bool parse_return_statement(Parser* p, ReturnStatement* st) {
    st->kind = SK_RETURN;
    if (!parse_expression(p, &st->expr, T_SEMICOLON)) {
        fprintf(stderr, "Failed to expression in return statement!\n");
        return false;
    }

    Token token = lexer_next_token(p->lex);
    if (token != T_SEMICOLON) {
        fprintf(stderr, "Expected semicolon after return statement, got %d!\n",
                token);
        return false;
    }

    return true;
}

bool parse_expr_statement(Parser* p, ExpressionStatement* st) {
    st->kind = SK_EXPRESSION;
    if (!parse_expression(p, &st->expr, T_SEMICOLON)) {
        fprintf(stderr,
                "Failed to parse expression in expression statement!\n");
        return false;
    }

    Token token = lexer_next_token(p->lex);
    if (token != T_SEMICOLON) {
        fprintf(stderr,
                "Expected semicolon after expression statement, got %d!\n",
                token);
        return false;
    }

    return true;
}

bool parse_statement(Parser* p, Statement* st) {
    st->kind = SK_EMPTY;

    Token token = lexer_next_token(p->lex);
    switch (token) {
        case T_OPEN_BRACKETS:
            if (!parse_block_statement(p, &st->block)) {
                fprintf(stderr, "Failed to parse block statement!\n");
                return false;
            }
            break;
        case KW_RETURN:
            if (!parse_return_statement(p, &st->ret)) {
                fprintf(stderr, "Failed to parse return statement!\n");
                return false;
            }
            break;
        case T_SEMICOLON:
            fprintf(stderr, "WARN:%zu:%zu: Extreanous semicolon!\n",
                    p->lex->token_start_loc.line, p->lex->token_start_loc.col);
            break;
        case T_IDENT: {
            Lexer backup = *p->lex;
            char* name = strndup(p->lex->token_text, p->lex->token_len);
            if (lexer_next_token(p->lex) == T_DECLARE) {
                *p->lex = backup;
                if (!parse_decl_statement(p, &st->expr, name)) {
                    fprintf(stderr, "Failed to parse local decl statement!\n");
                    return false;
                }
            } else {
                free(name);
                *p->lex = backup;
                lexer_undo_token(p->lex);
                if (!parse_expr_statement(p, &st->expr)) {
                    fprintf(stderr, "Failed to parse expr statement!\n");
                    return false;
                }
            }
        } break;
        default:
            fprintf(stderr, "Unexpected at beginning of statement %d: %.*s!\n",
                    token, (int)p->lex->token_len, p->lex->token_text);
            return false;
    }

    return true;
}

bool parse_global_scope(Parser* p) {
    while (true) {
        Token token = lexer_next_token(p->lex);
        switch (token) {
            case T_END:  // end of module
                return true;
            case KW_EXPORT:
                if (!parse_export_statement(p)) return false;
                break;
            case T_IDENT:
                if (!parse_decl_statement(
                        p, NULL,
                        strndup(p->lex->token_text, p->lex->token_len)))
                    return false;
                break;
            default:
                fprintf(stderr, "Unexpected token in global scope %d!\n",
                        token);
                return false;
        }
    }
}

Module parse(Lexer* lexer) {
    Module mod = {0};

    Parser parser = {.mod = &mod, .lex = lexer};

    da_append(mod.scopes, (DeclScope){0});

    if (!parse_global_scope(&parser)) {
        fprintf(stderr, "Failed to parse global scope!\n");
        exit(-1);
    }

    return mod;
}
