#include "lex.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char lexer_current_char(Lexer* lexer) {
    if (lexer->offset < lexer->input_size) {
        return lexer->input_buffer[lexer->offset];
    } else {
        return '\0';
    }
}

void lexer_consume_char(Lexer* lexer) {
    char c = lexer_current_char(lexer);
    switch (c) {
        case '\n':
            lexer->token_end_loc.line += 1;
            lexer->token_end_loc.col = 0;
            break;
        default:
            lexer->token_end_loc.col += 1;
    }
    lexer->offset++;
    lexer->token_len++;
}

Token lexer_next_token(Lexer* lexer) {
    while (isspace(lexer_current_char(lexer)) &&
           lexer->offset < lexer->input_size) {
        lexer_consume_char(lexer);
    }

    if (lexer->offset == lexer->input_size) return lexer->token = T_END;

    lexer->token_text = &lexer->input_buffer[lexer->offset];
    lexer->token_len = 0;
    lexer->token_start_loc = lexer->token_end_loc;

    if (isalpha(lexer_current_char(lexer))) {
        // indentifier or keyword
        while (isalnum(lexer_current_char(lexer))) {
            lexer_consume_char(lexer);
        }

        if (strncmp("fn", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_FN;
        }
        if (strncmp("export", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_EXPORT;
        }
        if (strncmp("return", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_RETURN;
        }
        if (strncmp("i32", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_i32;
        }

        return lexer->token = T_IDENT;
    } else if (isdigit(lexer_current_char(lexer))) {
        while (isalnum(lexer_current_char(lexer))) {
            lexer_consume_char(lexer);
        }

        int64_t res = 0;
        for (size_t i = 0; i < lexer->token_len; i++) {
            char c = lexer->token_text[i];
            if (!isdigit(c)) {
                fprintf(stderr, "%d:%d: Unsupported character in number: '%c'",
                        (int)lexer->token_start_loc.line,
                        (int)(lexer->token_start_loc.col + i), c);
                exit(-1);
            }
            res = res * 10 + (c - '0');
        }

        lexer->token_int = res;

        return lexer->token = T_INT;
    } else {
        switch (lexer_current_char(lexer)) {
            case ':':
                lexer_consume_char(lexer);
                if (lexer_current_char(lexer) == '=') {
                    lexer_consume_char(lexer);
                    return lexer->token = T_DECLARE;
                }
                return lexer->token = T_COLON;
            case ';':
                lexer_consume_char(lexer);
                return lexer->token = T_SEMICOLON;
            case '=':
                lexer_consume_char(lexer);
                return lexer->token = T_EQUAL;
            case ',':
                lexer_consume_char(lexer);
                return lexer->token = T_COMMA;
            case '{':
                lexer_consume_char(lexer);
                return lexer->token = T_OPEN_BRACKETS;
            case '}':
                lexer_consume_char(lexer);
                return lexer->token = T_CLOSE_BRACKETS;
            case '-':
                lexer_consume_char(lexer);
                if (lexer_current_char(lexer) == '>') {
                    lexer_consume_char(lexer);
                    return lexer->token = T_ARROW;
                }
                return lexer->token = T_MINUS;
            case '+':
                lexer_consume_char(lexer);
                return lexer->token = T_PLUS;
            default:
                fprintf(stderr, "%d:%d: Unknown token starting with: '%c'",
                        (int)lexer->token_start_loc.line,
                        (int)lexer->token_start_loc.col,
                        lexer_current_char(lexer));
                exit(-1);
        }
    }
}

void lexer_undo_token(Lexer* lexer) {
    lexer->token_end_loc = lexer->token_start_loc;
    lexer->offset -= lexer->token_len;
}
