#ifndef LEX_H_
#define LEX_H_

#include <stddef.h>
#include <stdint.h>

typedef enum {
    T_END = 0,
    T_IDENT,
    T_INT,
    T_COLON,
    T_SEMICOLON,
    T_ASSIGN,
    T_COMMA,
    T_ARROW,
    T_DECLARE,
    T_MINUS,
    T_PLUS,
    T_STAR,
    T_PERCENT,
    T_SLASH,
    T_OPEN_BRACKETS,
    T_CLOSE_BRACKETS,
    T_OPEN_PARENS,
    T_CLOSE_PARENS,

    KW_FN,
    KW_EXPORT,
    KW_EXTERN,
    KW_RETURN,

    KW_i32,
} Token;

typedef struct {
    size_t line;
    size_t col;
} Location;

typedef struct {
    char* input_buffer;
    size_t input_size;
    size_t offset;
    Token token;
    char* token_text;
    size_t token_len;
    int64_t token_int;
    Location token_start_loc;
    Location token_end_loc;
} Lexer;

Token lexer_next_token(Lexer* lexer);
void lexer_undo_token(Lexer* lexer);

#endif
