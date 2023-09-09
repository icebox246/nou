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

bool parse_expression(Parser* p, Expression* ex, Token terminator) {
    typedef struct {
        OperatorKind op;
    } Op;
    struct {
        da_list(Op);
    } op_stack = {0};

    while (true) {
        Token token = lexer_next_token(p->lex);
        if (token == terminator) {
            lexer_undo_token(p->lex);
            while (op_stack.count) {
                Expr e = {0};
                e.kind = EK_OPERATOR;
                e.props.op = op_stack.items[op_stack.count - 1].op;
                da_append(*ex, e);
                op_stack.count--;
            }
            free(op_stack.items);
            return true;
        }
        switch (token) {
            case T_IDENT: {  // TODO check if it is not a function call
                Expr e = {0};
                e.kind = EK_VAR;
                e.props.var = strndup(p->lex->token_text, p->lex->token_len);
                da_append(*ex, e);
            } break;
            case T_PLUS: {  // TODO operators should be treated in a special
                            // unique way
                da_append(op_stack, (Op){.op = OP_ADDITION});
            } break;
            case T_ASSIGN: {  
                da_append(op_stack, (Op){.op = OP_ASSIGNEMENT});
            } break;
            case T_INT: {  // TODO should all int consts be treated as i32?
                Expr e = {0};
                e.kind = EK_I32_CONST;
                e.props.i32 = p->lex->token_int;
                da_append(*ex, e);
            } break;
            default:
                fprintf(stderr, "Unexpected token in expression %d: `%.*s`!\n",
                        token, (int)p->lex->token_len, p->lex->token_text);
                return false;
        }
    }

    assert(false && "Unreachable");
}

bool parse_decl_statement(Parser* p, char* decl_name) {
    Decl d = {0};
    d.name = decl_name;

    Token token = lexer_next_token(p->lex);

    if (token != T_DECLARE) {
        fprintf(stderr, "Expected declare operator, got %d!\n", token);
        return false;
    }

    token = lexer_next_token(p->lex);
    switch (token) {
        case KW_FN: {
            d.kind = DK_FUNCTION;
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

            d.value = p->mod->functions.count;
            da_append(p->mod->functions, f);
        } break;
        default: {  // try to parse variable type
            lexer_undo_token(p->lex);
            d.kind = DK_VARIABLE;
            if (!parse_value_type(p, (ValueType*)&d.value)) {
                fprintf(stderr, "Failed to parse variable type!\n");
                return false;
            }
        }
    }

    token = lexer_next_token(p->lex);
    if (token != T_SEMICOLON) {
        fprintf(stderr, "Expected semicolon after decl, got %d!\n", token);
        return false;
    }

    da_append(p->mod->scopes.items[p->current_scope], d);
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
                if (!parse_decl_statement(p, name)) {
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
                        p, strndup(p->lex->token_text, p->lex->token_len)))
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
