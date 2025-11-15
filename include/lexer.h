#ifndef HCPL_LEXER_H
#define HCPL_LEXER_H

typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_TASK,
    TOKEN_LET,
    TOKEN_BE,
    TOKEN_IF,
    TOKEN_ELSE,

    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,

    TOKEN_ASSIGN,   // =
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_STAR,     // *
    TOKEN_SLASH     // /

} TokenType;

typedef struct {
    TokenType type;
    char* text;
} Token;

Token* lex_file(const char* filename);
void free_tokens(Token* tokens);

#endif
