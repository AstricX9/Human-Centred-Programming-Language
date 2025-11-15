#ifndef CORE_LEXER_H
#define CORE_LEXER_H

typedef enum {
	TOKEN_EOF,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_STRING,

	TOKEN_INCLUDE,
	TOKEN_PROGRAM,
	TOKEN_START,
	TOKEN_BUTTON,
	TOKEN_ON,
	TOKEN_PRESS,
	TOKEN_LET,
	TOKEN_BE,

	TOKEN_LBRACE,     // {
	TOKEN_RBRACE,     // }
	TOKEN_LPAREN,     // (
	TOKEN_RPAREN,     // )
	TOKEN_SEMICOLON,  // ;
	TOKEN_ASSIGN,     // =
	TOKEN_PLUS,       // +
	TOKEN_MINUS,      // -
	TOKEN_STAR,       // *
	TOKEN_SLASH       // /
} TokenType;

typedef struct {
	TokenType type;
	char* text;     // for identifier/string/number
} Token;

Token* lex_file(const char* filename);
void free_tokens(Token* tokens);
void print_tokens(Token* tokens);

#endif /* CORE_LEXER_H */
