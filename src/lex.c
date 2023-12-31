#include "lex.h"

#include <assert.h>
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

bool is_ident_char(char c) {
    switch (c) {
        case '_':
            return true;
        default:
            return isalnum(c);
    }
}

bool is_beginning_ident_char(char c) {
    switch (c) {
        case '_':
            return true;
        default:
            return isalpha(c);
    }
}

Token lexer_next_token(Lexer* lexer) {
    while (true) {
        bool something_was_done = false;
        while (isspace(lexer_current_char(lexer)) &&
               lexer->offset < lexer->input_size) {
            lexer_consume_char(lexer);
            something_was_done = true;
        }

        if (lexer->offset == lexer->input_size) return lexer->token = T_END;

        if (lexer->input_size - lexer->offset >= 2) {
            if (lexer_current_char(lexer) == '/' &&
                lexer->input_buffer[lexer->offset + 1] == '/') {
                // comment
                while (lexer_current_char(lexer) != '\n') {
                    lexer_consume_char(lexer);
                    something_was_done = true;
                }
            }
        }

        if (lexer->offset == lexer->input_size) return lexer->token = T_END;

        if (!something_was_done) break;
    }

    lexer->token_text = &lexer->input_buffer[lexer->offset];
    lexer->token_len = 0;
    lexer->token_start_loc = lexer->token_end_loc;

    if (is_beginning_ident_char(lexer_current_char(lexer))) {
        // indentifier or keyword
        while (is_ident_char(lexer_current_char(lexer))) {
            lexer_consume_char(lexer);
        }

        if (lexer->token_len == 2 &&
            strncmp("as", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_AS;
        }
        if (lexer->token_len == 2 &&
            strncmp("fn", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_FN;
        }
        if (lexer->token_len == 2 &&
            strncmp("if", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_IF;
        }
        if (lexer->token_len == 4 &&
            strncmp("else", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_ELSE;
        }
        if (lexer->token_len == 6 &&
            strncmp("export", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_EXPORT;
        }
        if (lexer->token_len == 6 &&
            strncmp("extern", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_EXTERN;
        }
        if (lexer->token_len == 6 &&
            strncmp("return", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_RETURN;
        }
        if (lexer->token_len == 2 &&
            strncmp("u8", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_u8;
        }
        if (lexer->token_len == 3 &&
            strncmp("i32", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_i32;
        }
        if (lexer->token_len == 3 &&
            strncmp("u32", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_u32;
        }
        if (lexer->token_len == 4 &&
            strncmp("bool", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_bool;
        }
        if (lexer->token_len == 3 &&
            strncmp("and", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_AND;
        }
        if (lexer->token_len == 2 &&
            strncmp("or", lexer->token_text, lexer->token_len) == 0) {
            return lexer->token = KW_OR;
        }
        if (lexer->token_len == 4 &&
            strncmp("true", lexer->token_text, lexer->token_len) == 0) {
            lexer->token_bool = true;
            return lexer->token = T_BOOL;
        }
        if (lexer->token_len == 5 &&
            strncmp("false", lexer->token_text, lexer->token_len) == 0) {
            lexer->token_bool = false;
            return lexer->token = T_BOOL;
        }

        return lexer->token = T_IDENT;
    } else if (isdigit(lexer_current_char(lexer))) {
        while (isalnum(lexer_current_char(lexer))) {
            lexer_consume_char(lexer);
        }

        int64_t res = 0;
        bool parsing_bits = false;

        int res_bits = 32;  // default type is i32
        bool res_unsing = false;

        for (size_t i = 0; i < lexer->token_len; i++) {
            char c = lexer->token_text[i];
            if (!isdigit(c)) {
                if (!parsing_bits && (c == 'u' || c == 'i')) {
                    res_bits = 0;
                    res_unsing = c == 'u';
                    parsing_bits = true;
                    continue;
                }
                loc_print(stderr, lexer->token_start_loc);
                fprintf(stderr, "Unsupported character in number: '%c'\n", c);
                exit(-1);
            }
            if (!parsing_bits) {
                res = res * 10 + (c - '0');
            } else {
                res_bits = res_bits * 10 + (c - '0');
            }
        }

        lexer->token_int = res;
        lexer->token_bits = res_bits;
        lexer->token_unsign = res_unsing;

        return lexer->token = T_INT;
    } else if (lexer_current_char(lexer) == '"') {
        lexer->token_str.count = 0;
        bool escaped;
        do {
            escaped = false;
            lexer_consume_char(lexer);
            if (lexer_current_char(lexer) == '\0') {
                loc_print(stderr, lexer->token_start_loc);
                fprintf(stderr,
                        "Reached end of file in before string literal end\n");
                exit(-1);
            }
            if (lexer_current_char(lexer) == '\\') {
                lexer_consume_char(lexer);
                char c = lexer_current_char(lexer);
                escaped = true;
                switch (c) {
                    case 'n':
                        da_append(lexer->token_str, '\n');
                        break;
                    default:
                        da_append(lexer->token_str, c);
                        break;
                }
            } else {
                da_append(lexer->token_str, lexer_current_char(lexer));
            }
        } while (lexer_current_char(lexer) != '"' || escaped);

        lexer_consume_char(lexer);  // consume closing "
        lexer->token_str.count--;   // drop closing "

        return lexer->token = T_STRING;
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
                if (lexer_current_char(lexer) == '=') {
                    lexer_consume_char(lexer);
                    return lexer->token = T_EQUAL;
                }
                return lexer->token = T_ASSIGN;
            case ',':
                lexer_consume_char(lexer);
                return lexer->token = T_COMMA;
            case '{':
                lexer_consume_char(lexer);
                return lexer->token = T_OPEN_BRACKETS;
            case '}':
                lexer_consume_char(lexer);
                return lexer->token = T_CLOSE_BRACKETS;
            case '[':
                lexer_consume_char(lexer);
                return lexer->token = T_OPEN_SQUARE;
            case ']':
                lexer_consume_char(lexer);
                return lexer->token = T_CLOSE_SQUARE;
            case '(':
                lexer_consume_char(lexer);
                return lexer->token = T_OPEN_PARENS;
            case ')':
                lexer_consume_char(lexer);
                return lexer->token = T_CLOSE_PARENS;
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
            case '*':
                lexer_consume_char(lexer);
                return lexer->token = T_STAR;
            case '%':
                lexer_consume_char(lexer);
                return lexer->token = T_PERCENT;
            case '/':
                lexer_consume_char(lexer);
                return lexer->token = T_SLASH;
            case '!':
                lexer_consume_char(lexer);
                return lexer->token = T_EXCLAMATION;
            case '.':
                lexer_consume_char(lexer);
                return lexer->token = T_DOT;
            default:
                fprintf(stderr, "%d:%d: Unknown token starting with: '%c'\n",
                        (int)lexer->token_start_loc.line,
                        (int)lexer->token_start_loc.col,
                        lexer_current_char(lexer));
                exit(-1);
        }
    }
    assert(false && "Unreachable");
}

void lexer_undo_token(Lexer* lexer) {
    lexer->token_end_loc = lexer->token_start_loc;
    lexer->offset -= lexer->token_len;
}

void loc_print(FILE* fd, Location loc) {
    fprintf(fd, "%zu:%zu: ", loc.line, loc.col);
}
