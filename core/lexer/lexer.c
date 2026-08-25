#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* source;
    int         length;
    int         index;
    int         line;
    int         line_start;   /* offset of the first byte on the current line */

    Token*      tokens;
    int         count;
    int         capacity;

    Arena*      arena;
    DiagList*   diags;
} LexerState;

typedef struct {
    const char* word;
    TokenType   type;
} Keyword;

/* Longest-match is handled by the identifier scanner; this table only has to
   answer whether an exact lexeme is reserved. */
static const Keyword KEYWORDS[] = {
    { "include",  TOKEN_INCLUDE  },
    { "program",  TOKEN_PROGRAM  },
    { "start",    TOKEN_START    },
    { "task",     TOKEN_TASK     },
    { "function", TOKEN_FUNCTION },
    { "button",   TOKEN_BUTTON   },
    { "on",       TOKEN_ON       },
    { "let",      TOKEN_LET      },
    { "be",       TOKEN_BE       },
    { "set",      TOKEN_SET      },
    { "to",       TOKEN_TO       },
    { "return",   TOKEN_RETURN   },
    { "if",       TOKEN_IF       },
    { "else",     TOKEN_ELSE     },
    { "print",    TOKEN_PRINT    },
    { "increase", TOKEN_INCREASE },
    { "reduce",   TOKEN_REDUCE   },
    { "multiply", TOKEN_MULTIPLY },
    { "divide",   TOKEN_DIVIDE   },
    { "by",       TOKEN_BY       },
    { "is",       TOKEN_IS       },
    { "not",      TOKEN_NOT      },
    { "equal",    TOKEN_EQUAL    },
    { "greater",  TOKEN_GREATER  },
    { "less",     TOKEN_LESS     },
    { "than",     TOKEN_THAN     },
    { "and",      TOKEN_AND      },
    { "or",       TOKEN_OR       }
};

static const char* TOKEN_NAMES[TOKEN_TYPE_COUNT] = {
    "EOF",
    "IDENTIFIER", "NUMBER", "STRING", "BOOLEAN", "DURATION",
    "include", "program", "start", "task", "function", "button", "on",
    "let", "be", "set", "to", "return", "if", "else", "print",
    "increase", "reduce", "multiply", "divide", "by",
    "is", "not", "equal", "greater", "less", "than",
    "and", "or",
    "{", "}", "(", ")", ";", ",", ".", "+", "-", "*", "/", "="
};

const char* token_type_name(TokenType type) {
    if (type < 0 || type >= TOKEN_TYPE_COUNT) return "?";
    return TOKEN_NAMES[type];
}

/* ---------------------------------------------------------------- scanning */

static char peek_at(LexerState* ls, int ahead) {
    int index = ls->index + ahead;
    if (index >= ls->length) return '\0';
    return ls->source[index];
}

static char peek_char(LexerState* ls) {
    return peek_at(ls, 0);
}

static int current_column(LexerState* ls) {
    return (ls->index - ls->line_start) + 1;
}

static void advance_char(LexerState* ls) {
    if (ls->index >= ls->length) return;

    if (ls->source[ls->index] == '\n') {
        ls->line++;
        ls->line_start = ls->index + 1;
    }
    ls->index++;
}

static int is_identifier_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_identifier_part(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '.';
}

/* --------------------------------------------------------------- emitting */

static void push_token(LexerState* ls, TokenType type, char* text,
                       int line, int column, int offset) {
    Token* token;

    if (ls->count >= ls->capacity) {
        /* Abandoning the old array inside the arena is intentional; see arena.h. */
        int    grown = ls->capacity ? ls->capacity * 2 : 256;
        Token* moved = (Token*)arena_alloc(ls->arena, sizeof(Token) * (size_t)grown);
        if (!moved) return;

        if (ls->count) memcpy(moved, ls->tokens, sizeof(Token) * (size_t)ls->count);
        ls->tokens   = moved;
        ls->capacity = grown;
    }

    token = &ls->tokens[ls->count++];
    token->type   = type;
    token->text   = text;
    token->line   = line;
    token->column = column;
    token->offset = offset;
    token->length = ls->index - offset;
    if (token->length < 1) token->length = 1;
}

static void push_simple(LexerState* ls, TokenType type) {
    int line   = ls->line;
    int column = current_column(ls);
    int offset = ls->index;

    advance_char(ls);
    push_token(ls, type, arena_strdup(ls->arena, token_type_name(type)), line, column, offset);
}

/* ---------------------------------------------------------------- literals */

/* Comments are skipped here rather than emitted, which keeps the slash
   unambiguous for the division operator. */
static int skip_comment(LexerState* ls) {
    if (peek_char(ls) != '/') return 0;

    if (peek_at(ls, 1) == '/') {
        while (ls->index < ls->length && peek_char(ls) != '\n') advance_char(ls);
        return 1;
    }

    if (peek_at(ls, 1) == '*') {
        int line   = ls->line;
        int column = current_column(ls);

        advance_char(ls);
        advance_char(ls);

        while (ls->index < ls->length) {
            if (peek_char(ls) == '*' && peek_at(ls, 1) == '/') {
                advance_char(ls);
                advance_char(ls);
                return 1;
            }
            advance_char(ls);
        }

        diag_add(ls->diags, DIAG_LEXER, line, column, 2, "unterminated block comment");
        return 1;
    }

    return 0;
}

static void scan_identifier(LexerState* ls) {
    int    line   = ls->line;
    int    column = current_column(ls);
    int    offset = ls->index;
    char*  text;
    size_t i;

    while (ls->index < ls->length && is_identifier_part(peek_char(ls))) advance_char(ls);

    text = arena_strndup(ls->arena, ls->source + offset, (size_t)(ls->index - offset));

    if (strcmp(text, "yes") == 0 || strcmp(text, "no") == 0) {
        push_token(ls, TOKEN_BOOLEAN, text, line, column, offset);
        return;
    }

    for (i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        if (strcmp(text, KEYWORDS[i].word) == 0) {
            push_token(ls, KEYWORDS[i].type, text, line, column, offset);
            return;
        }
    }

    push_token(ls, TOKEN_IDENTIFIER, text, line, column, offset);
}

/* A trailing s/m/h makes a duration, but only when nothing identifier-like
   follows it, so 30s is a duration while 2max is 2 followed by max. */
static int duration_suffix_follows(LexerState* ls) {
    char suffix = peek_char(ls);
    char after;

    if (suffix != 's' && suffix != 'm' && suffix != 'h') return 0;

    after = peek_at(ls, 1);
    return !is_identifier_part(after);
}

static void scan_number(LexerState* ls) {
    int   line   = ls->line;
    int   column = current_column(ls);
    int   offset = ls->index;
    char* text;

    while (ls->index < ls->length && isdigit((unsigned char)peek_char(ls))) advance_char(ls);

    /* A dot only continues the number when a digit follows it, so 14.5 is one
       number while 14.foo is a number followed by a stray dot. */
    if (peek_char(ls) == '.' && isdigit((unsigned char)peek_at(ls, 1))) {
        advance_char(ls);
        while (ls->index < ls->length && isdigit((unsigned char)peek_char(ls))) advance_char(ls);
    }

    if (peek_char(ls) == 'e' || peek_char(ls) == 'E') {
        int sign_offset = (peek_at(ls, 1) == '+' || peek_at(ls, 1) == '-') ? 2 : 1;
        if (isdigit((unsigned char)peek_at(ls, sign_offset))) {
            advance_char(ls);
            if (sign_offset == 2) advance_char(ls);
            while (ls->index < ls->length && isdigit((unsigned char)peek_char(ls))) advance_char(ls);
        }
    }

    if (duration_suffix_follows(ls)) {
        advance_char(ls);
        text = arena_strndup(ls->arena, ls->source + offset, (size_t)(ls->index - offset));
        push_token(ls, TOKEN_DURATION, text, line, column, offset);
        return;
    }

    text = arena_strndup(ls->arena, ls->source + offset, (size_t)(ls->index - offset));
    push_token(ls, TOKEN_NUMBER, text, line, column, offset);
}

/* Escapes are decoded here so the parser and runtime never see backslashes.
   The token length still spans the raw source, which keeps carets aligned. */
static void scan_string(LexerState* ls) {
    int   line   = ls->line;
    int   column = current_column(ls);
    int   offset = ls->index;
    char* buffer;
    int   written  = 0;
    int   reported = 0;

    advance_char(ls); /* opening quote */

    buffer = (char*)arena_alloc(ls->arena, (size_t)(ls->length - ls->index) + 1u);

    while (ls->index < ls->length && peek_char(ls) != '"') {
        char c = peek_char(ls);

        if (c == '\n') {
            diag_add(ls->diags, DIAG_LEXER, line, column, 1,
                     "unterminated string literal");
            reported = 1;
            break;
        }

        if (c == '\\') {
            char escaped = peek_at(ls, 1);
            advance_char(ls);

            switch (escaped) {
                case 'n':  buffer[written++] = '\n'; break;
                case 't':  buffer[written++] = '\t'; break;
                case 'r':  buffer[written++] = '\r'; break;
                case '\\': buffer[written++] = '\\'; break;
                case '"':  buffer[written++] = '"';  break;
                default:
                    diag_add(ls->diags, DIAG_LEXER, ls->line, current_column(ls), 2,
                             "unknown escape sequence in string literal");
                    buffer[written++] = escaped;
                    break;
            }
            advance_char(ls);
            continue;
        }

        buffer[written++] = c;
        advance_char(ls);
    }

    if (peek_char(ls) == '"') {
        advance_char(ls);
    } else if (!reported) {
        diag_add(ls->diags, DIAG_LEXER, line, column, 1, "unterminated string literal");
    }

    buffer[written] = '\0';
    push_token(ls, TOKEN_STRING, buffer, line, column, offset);
}

/* ------------------------------------------------------------------ driver */

static char* read_file(Arena* arena, const char* filename) {
    FILE*  file = fopen(filename, "rb");
    long   size;
    char*  buffer;
    size_t read;

    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    size = ftell(file);
    if (size < 0) { fclose(file); return NULL; }
    rewind(file);

    buffer = (char*)arena_alloc(arena, (size_t)size + 1u);
    if (!buffer) { fclose(file); return NULL; }

    read = fread(buffer, 1, (size_t)size, file);
    buffer[read] = '\0';

    fclose(file);
    return buffer;
}

int lex_source(Arena* arena, const char* source, const char* filename,
               TokenStream* out, DiagList* diags) {
    LexerState ls;

    memset(&ls, 0, sizeof(ls));
    ls.source     = source;
    ls.length     = (int)strlen(source);
    ls.line       = 1;
    ls.line_start = 0;
    ls.arena      = arena;
    ls.diags      = diags;

    while (ls.index < ls.length) {
        char c = peek_char(&ls);

        if (isspace((unsigned char)c))     { advance_char(&ls);    continue; }
        if (c == '/' && skip_comment(&ls))                         continue;

        if (is_identifier_start(c))        { scan_identifier(&ls); continue; }
        if (isdigit((unsigned char)c))     { scan_number(&ls);     continue; }
        if (c == '"')                      { scan_string(&ls);     continue; }

        switch (c) {
            case '{': push_simple(&ls, TOKEN_LBRACE);    continue;
            case '}': push_simple(&ls, TOKEN_RBRACE);    continue;
            case '(': push_simple(&ls, TOKEN_LPAREN);    continue;
            case ')': push_simple(&ls, TOKEN_RPAREN);    continue;
            case ';': push_simple(&ls, TOKEN_SEMICOLON); continue;
            case ',': push_simple(&ls, TOKEN_COMMA);     continue;
            case '.': push_simple(&ls, TOKEN_DOT);       continue;
            case '+': push_simple(&ls, TOKEN_PLUS);      continue;
            case '-': push_simple(&ls, TOKEN_MINUS);     continue;
            case '*': push_simple(&ls, TOKEN_STAR);      continue;
            case '/': push_simple(&ls, TOKEN_SLASH);     continue;
            case '=': push_simple(&ls, TOKEN_ASSIGN);    continue;
            default:
                diag_add(diags, DIAG_LEXER, ls.line, current_column(&ls), 1,
                         "unexpected character in source");
                advance_char(&ls);
                continue;
        }
    }

    push_token(&ls, TOKEN_EOF, arena_strdup(arena, "end of file"),
               ls.line, current_column(&ls), ls.index);

    out->tokens   = ls.tokens;
    out->count    = ls.count;
    out->source   = (char*)source;
    out->filename = filename;
    return 1;
}

int lex_file(Arena* arena, const char* filename, TokenStream* out, DiagList* diags) {
    char* source = read_file(arena, filename);
    if (!source) return 0;
    return lex_source(arena, source, filename, out, diags);
}

void print_tokens(const TokenStream* stream) {
    int i;

    printf("%-5s %-7s %-12s %s\n", "#", "line:col", "type", "text");
    printf("----- ------- ------------ -----------------------------------\n");

    for (i = 0; i < stream->count; i++) {
        const Token* token = &stream->tokens[i];
        printf("%-5d %3d:%-3d %-12s %s\n",
               i, token->line, token->column,
               token_type_name(token->type),
               token->text ? token->text : "");
    }
}
