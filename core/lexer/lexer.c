#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char* source;
    int index;
    int length;

    Token* tokens;
    int token_count;
} LexerState;

static char* make_str(const char* src, int len) {
    char* s = malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, src, len);
    s[len] = '\0';
    return s;
}

// Helper: create and push token
void push_token(LexerState* ls, TokenType type, const char* text) {
    Token t;
    t.type = type;
    t.text = text ? strdup(text) : NULL;

    ls->tokens[ls->token_count++] = t;
}

// Read whole file
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

// Check if identifier is a keyword
TokenType keyword_type(const char* text) {
    if (strcmp(text, "include") == 0) return TOKEN_INCLUDE;
    if (strcmp(text, "program") == 0) return TOKEN_PROGRAM;
    if (strcmp(text, "start")   == 0) return TOKEN_START;
    if (strcmp(text, "button")  == 0) return TOKEN_BUTTON;
    if (strcmp(text, "on")      == 0) return TOKEN_ON;
    if (strcmp(text, "press")   == 0) return TOKEN_PRESS;
    if (strcmp(text, "let")     == 0) return TOKEN_LET;
    if (strcmp(text, "be")      == 0) return TOKEN_BE;

    return TOKEN_IDENTIFIER;
}

// Main lexing loop
Token* lex_file(const char* filename) {

    LexerState ls = {0};
    ls.source = read_file(filename);
    if (!ls.source) {
        printf("Cannot open file: %s\n", filename);
        return NULL;
    }

    ls.index = 0;
    ls.length = (int)strlen(ls.source);

    // MAX TOKENS = file length (worst case)
    ls.tokens = malloc(sizeof(Token) * ls.length);
    ls.token_count = 0;

    while (ls.index < ls.length) {
        char c = ls.source[ls.index];

        // Skip whitespace
        if (isspace((unsigned char)c)) {
            ls.index++;
            continue;
        }

        // Identifiers or keywords (allow underscore)
        if (isalpha((unsigned char)c)) {
            int start = ls.index;

            /* allow letters, digits, underscore and dot in identifiers (dotted namespaces) */
            while (isalnum((unsigned char)ls.source[ls.index]) || ls.source[ls.index] == '_' || ls.source[ls.index] == '.')
                ls.index++;

            int len = ls.index - start;
            char* text = make_str(ls.source + start, len);

            TokenType type = keyword_type(text);
            push_token(&ls, type, text);

            free(text);
            continue;
        }

        // Numbers
        if (isdigit((unsigned char)c)) {
            int start = ls.index;

            while (isdigit((unsigned char)ls.source[ls.index]))
                ls.index++;

            char* num = make_str(ls.source + start, ls.index - start);
            push_token(&ls, TOKEN_NUMBER, num);
            free(num);
            continue;
        }

        // Strings
        if (c == '"') {
            ls.index++; // skip quote
            int start = ls.index;

            while (ls.source[ls.index] != '"' && ls.index < ls.length)
                ls.index++;

            int len = ls.index - start;
            char* str = make_str(ls.source + start, len);

            push_token(&ls, TOKEN_STRING, str);
            free(str);

            ls.index++; // skip closing quote
            continue;
        }

        // Single-character tokens
        switch (c) {
            case '{': push_token(&ls, TOKEN_LBRACE, "{"); break;
            case '}': push_token(&ls, TOKEN_RBRACE, "}"); break;
            case '(' : push_token(&ls, TOKEN_LPAREN, "("); break;
            case ')': push_token(&ls, TOKEN_RPAREN, ")"); break;
            case ';': push_token(&ls, TOKEN_SEMICOLON, ";"); break;
            case '+': push_token(&ls, TOKEN_PLUS, "+"); break;
            case '-': push_token(&ls, TOKEN_MINUS, "-"); break;
            case '*': push_token(&ls, TOKEN_STAR, "*"); break;
            case '/': push_token(&ls, TOKEN_SLASH, "/"); break;
            case '=': push_token(&ls, TOKEN_ASSIGN, "="); break;
        }

        ls.index++;
    }

    push_token(&ls, TOKEN_EOF, NULL);
    return ls.tokens;

// Print tokens for debugging
}

void print_tokens(Token* tokens) {
    int i = 0;
    while (tokens[i].type != TOKEN_EOF) {
        printf("Token %d: type=%d", i, tokens[i].type);

        if (tokens[i].text)
            printf(", text=\"%s\"", tokens[i].text);

        printf("\n");
        i++;
    }
    // Print EOF as well
    printf("Token %d: type=%d\n", i, tokens[i].type);
}

// Free tokens
void free_tokens(Token* tokens) {
    // free text fields
    int i = 0;
    while (tokens[i].type != TOKEN_EOF) {
        if (tokens[i].text) free(tokens[i].text);
        i++;
    }
    free(tokens);
}
