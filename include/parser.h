#ifndef HCPL_PARSER_H
#define HCPL_PARSER_H

/*
 * Recursive-descent parser, per docs/v0_0_1/language.md section 2.
 *
 * Errors are collected, not thrown: on a parse error the parser records a
 * diagnostic and synchronises to the next `;` or `}` so a single run reports
 * every problem in the file. A non-NULL result can therefore still accompany a
 * non-zero diags->count -- check the diagnostics, not the pointer.
 */

#include "arena.h"
#include "ast.h"
#include "diag.h"
#include "lexer.h"

/* Parses a token stream into a NODE_UNIT. Never returns NULL for a
   well-formed stream; the unit may contain fewer declarations than the source
   if errors were skipped. */
Node* parse_unit(Arena* arena, const TokenStream* stream, DiagList* diags);

#endif /* HCPL_PARSER_H */
