#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';

    fclose(f);
    return buffer;
}

Token* lex_file(const char* filename) {

    char* source = read_file(filename);
    if (!source) {
        printf("Cannot open file: %s\n", filename);
        return NULL;
    }

    // Temporary: just return EOF token
    Token* tokens = malloc(sizeof(Token) * 2);
    tokens[0].type = TOKEN_EOF;
    tokens[0].text = NULL;
    tokens[1].type = TOKEN_EOF;
    tokens[1].text = NULL;

    free(source);
    return tokens;
}

void free_tokens(Token* tokens) {
    free(tokens);
}
