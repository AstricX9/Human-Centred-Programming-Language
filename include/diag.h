#ifndef HCPL_DIAG_H
#define HCPL_DIAG_H

/*
 * Diagnostics carry a phase and a source range so they can be asserted on in
 * tests, per docs/v0_0_1/language.md section 6. Errors are collected rather
 * than thrown: the parser synchronises and keeps going so one run reports
 * every problem in the file, not just the first.
 */

#include "arena.h"

typedef enum {
    DIAG_LEXER,
    DIAG_PARSER,
    DIAG_SEMANTIC,
    DIAG_RUNTIME
} DiagPhase;

typedef struct Diag Diag;

struct Diag {
    DiagPhase phase;
    int       line;
    int       column;
    int       length;
    char*     message;
    Diag*     next;
};

typedef struct {
    Arena*      arena;
    const char* filename;
    Diag*       first;
    Diag*       last;
    int         count;
} DiagList;

void        diag_init(DiagList* diags, Arena* arena, const char* filename);
void        diag_add(DiagList* diags, DiagPhase phase, int line, int column, int length,
                     const char* format, ...);
void        diag_print_all(const DiagList* diags, const char* source);
const char* diag_phase_name(DiagPhase phase);

#endif /* HCPL_DIAG_H */
