#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void diag_init(DiagList* diags, Arena* arena, const char* filename) {
    diags->arena    = arena;
    diags->filename = filename;
    diags->first    = NULL;
    diags->last     = NULL;
    diags->count    = 0;
}

void diag_add(DiagList* diags, DiagPhase phase, int line, int column, int length,
              const char* format, ...) {
    va_list args;
    Diag*   diag;

    if (!diags) return;

    diag = (Diag*)arena_zalloc(diags->arena, sizeof(Diag));
    if (!diag) return;

    diag->phase  = phase;
    diag->line   = line;
    diag->column = column;
    diag->length = length > 0 ? length : 1;

    va_start(args, format);
    diag->message = arena_vsprintf(diags->arena, format, args);
    va_end(args);

    if (diags->last) {
        diags->last->next = diag;
    } else {
        diags->first = diag;
    }
    diags->last = diag;
    diags->count++;
}

const char* diag_phase_name(DiagPhase phase) {
    switch (phase) {
        case DIAG_LEXER:    return "lex";
        case DIAG_PARSER:   return "parse";
        case DIAG_SEMANTIC: return "semantic";
        case DIAG_RUNTIME:  return "runtime";
        default:            return "error";
    }
}

/* Locate the 1-based line `line` inside `source` and print it with a caret
   under the offending column. This is the whole point of tracking positions in
   the lexer, so it is worth the few lines here. */
static void print_source_line(const char* source, int line, int column, int length) {
    const char* cursor  = source;
    const char* start;
    const char* end;
    int         current = 1;
    int         i;

    if (!source || line <= 0) return;

    while (current < line && *cursor) {
        if (*cursor == '\n') current++;
        cursor++;
    }
    if (current != line) return;

    start = cursor;
    end   = cursor;
    while (*end && *end != '\n' && *end != '\r') end++;

    printf("   %5d | %.*s\n", line, (int)(end - start), start);
    printf("         | ");

    for (i = 1; i < column; i++) putchar(' ');
    for (i = 0; i < length; i++) putchar('^');
    putchar('\n');
}

/* Lexing finishes before parsing starts, so the list arrives grouped by phase
   rather than by position: a lex error on line 19 would print above a parse
   error on line 1. Reading errors top to bottom matters more than knowing
   which pass found them. Insertion sort is stable, so two errors at the same
   position keep the order they were found in. */
static const Diag** diags_sorted_by_position(const DiagList* diags) {
    const Diag** ordered;
    const Diag*  diag;
    int          count = 0;

    ordered = (const Diag**)arena_alloc(diags->arena, sizeof(Diag*) * (size_t)diags->count);
    if (!ordered) return NULL;

    for (diag = diags->first; diag; diag = diag->next) {
        int i = count++;

        while (i > 0 &&
               (ordered[i - 1]->line > diag->line ||
                (ordered[i - 1]->line == diag->line &&
                 ordered[i - 1]->column > diag->column))) {
            ordered[i] = ordered[i - 1];
            i--;
        }
        ordered[i] = diag;
    }

    return ordered;
}

void diag_print_all(const DiagList* diags, const char* source) {
    const Diag** ordered;
    int          i;

    if (!diags || diags->count == 0) return;

    ordered = diags_sorted_by_position(diags);
    if (!ordered) return;

    for (i = 0; i < diags->count; i++) {
        const Diag* diag = ordered[i];

        printf("%s:%d:%d: %s error: %s\n",
               diags->filename ? diags->filename : "<input>",
               diag->line, diag->column,
               diag_phase_name(diag->phase),
               diag->message ? diag->message : "(no message)");
        print_source_line(source, diag->line, diag->column, diag->length);
    }

    printf("\n%d error%s.\n", diags->count, diags->count == 1 ? "" : "s");
}
