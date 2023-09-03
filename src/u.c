#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "lex.h"
#include "mod_vis.h"
#include "parse.h"

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "input file must be provided");
        return -1;
    }

    argv++;

    char* input_file_name = *argv;

    FILE* input_file = fopen(input_file_name, "r");
    fseek(input_file, 0, SEEK_END);
    size_t input_file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    char* input_file_buffer = malloc(input_file_size + 1);
    fread(input_file_buffer, input_file_size, 1, input_file);
    fclose(input_file);

    // TODO add flags which enables displaying tokens
    /* { */
    /*     Lexer lexer = { */
    /*         .input_buffer = input_file_buffer, */
    /*         .input_size = input_file_size, */
    /*     }; */

    /*     Token token; */
    /*     while ((token = lexer_next_token(&lexer)) != T_END) { */
    /*         printf("%d:%d: Token: %d %.*s\n",
     * (int)lexer.token_start_loc.line, */
    /*                (int)lexer.token_start_loc.col, token,
     * (int)lexer.token_len, */
    /*                lexer.token_text); */
    /*     } */
    /* } */

    {
        Lexer lexer = {
            .input_buffer = input_file_buffer,
            .input_size = input_file_size,
        };

        Module mod = parse(&lexer);

        /* visualize_module(&mod, stdout); */

        ByteBuffer output = codegen_module(&mod);

        fprintf(stderr, "INFO: Writing to a.out\n");
        FILE* output_file = fopen("a.out", "w");
        fwrite(output.items, sizeof(uint8_t), output.count, output_file);
        fclose(output_file);
    }

    free(input_file_buffer);
    return 0;
}
