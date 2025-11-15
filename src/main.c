#include <stdio.h>
#include <string.h>
#include "lexer.h"

void hcpl_run(const char* filename) {
    printf("[HCPL] Running file: %s\n", filename);

    // For now, just call the lexer (it will do nothing yet)
    Token* tokens = lex_file(filename);

    if (!tokens) {
        printf("[HCPL] Failed to lex file.\n");
        return;
    }

    printf("[HCPL] Lexing complete. Tokens:\n");
    print_tokens(tokens);

    free_tokens(tokens);
}

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("Usage: hcpl run <file>\n");
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        hcpl_run(argv[2]);
        return 0;
    }

    printf("Unknown command: %s\n", argv[1]);
    return 1;
}
