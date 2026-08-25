#ifndef HCPL_LEXER_H
#define HCPL_LEXER_H

/*
 * HCPL tokenizer. Follows docs/v0_0_1/language.md section 1: every token
 * carries its source position, keywords are dedicated token types, and the
 * longest-match rule decides between an identifier and a keyword.
 *
 * Identifiers may contain dots so that `math.add` and `system.io.write` arrive
 * as a single token. That is why TOKEN_DOT is almost never produced -- it
 * exists so a stray dot can be reported rather than silently dropped.
 */

#include "arena.h"
#include "diag.h"

typedef enum {
    TOKEN_EOF = 0,

    /* literals */
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_BOOLEAN,      /* yes / no */
    TOKEN_DURATION,     /* 30s / 2m / 1h */

    /* declaration keywords */
    TOKEN_INCLUDE,
    TOKEN_PROGRAM,
    TOKEN_START,
    TOKEN_TASK,
    TOKEN_FUNCTION,
    TOKEN_BUTTON,
    TOKEN_ON,

    /* statement keywords */
    TOKEN_LET,
    TOKEN_BE,
    TOKEN_SET,
    TOKEN_TO,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_PRINT,

    /* natural arithmetic */
    TOKEN_INCREASE,
    TOKEN_REDUCE,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_BY,

    /* comparison words */
    TOKEN_IS,
    TOKEN_NOT,
    TOKEN_EQUAL,
    TOKEN_GREATER,
    TOKEN_LESS,
    TOKEN_THAN,

    /* connectors */
    TOKEN_AND,
    TOKEN_OR,

    /* punctuation and operators */
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_ASSIGN,

    TOKEN_TYPE_COUNT
} TokenType;

typedef struct {
    TokenType type;
    char*     text;     /* arena-owned; string literals hold decoded content */
    int       line;     /* 1-based */
    int       column;   /* 1-based */
    int       offset;   /* byte offset into source */
    int       length;   /* raw source length, for caret underlines */
} Token;

typedef struct {
    Token*      tokens;
    int         count;
    char*       source;     /* whole file, arena-owned, NUL-terminated */
    const char* filename;
} TokenStream;

/* Lexes `filename` into `out`. All memory is drawn from `arena`; nothing here
   needs freeing individually. Returns 1 if the file was read, 0 if it could
   not be opened. Lexical errors are appended to `diags` and do not stop the
   scan -- an unterminated string still yields a usable token stream. */
int lex_file(Arena* arena, const char* filename, TokenStream* out, DiagList* diags);

/* Same, for an in-memory buffer. `source` is copied into the arena. */
int lex_source(Arena* arena, const char* source, const char* filename,
               TokenStream* out, DiagList* diags);

void        print_tokens(const TokenStream* stream);
const char* token_type_name(TokenType type);

#endif /* HCPL_LEXER_H */
