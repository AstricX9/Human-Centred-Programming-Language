#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "diag.h"
#include "lexer.h"
#include "parser.h"

typedef struct {
    const char* filename;
    int         show_tokens;
    int         show_ast;
} Options;

static void print_usage(void) {
    printf("Usage: hcpl run <file.hpl> [--tokens] [--no-ast]\n");
    printf("\n");
    printf("  --tokens   dump the token stream before parsing\n");
    printf("  --no-ast   parse and report errors without printing the tree\n");
}

static int hcpl_run(const Options* options) {
    Arena*      arena = arena_create(0);
    DiagList    diags;
    TokenStream stream;
    Node*       unit;
    int         status = 0;

    if (!arena) {
        printf("[hcpl] out of memory\n");
        return 1;
    }

    diag_init(&diags, arena, options->filename);

    if (!lex_file(arena, options->filename, &stream, &diags)) {
        printf("[hcpl] cannot open file: %s\n", options->filename);
        arena_destroy(arena);
        return 1;
    }

    if (options->show_tokens) {
        print_tokens(&stream);
        printf("\n");
    }

    unit = parse_unit(arena, &stream, &diags);

    if (diags.count > 0) {
        diag_print_all(&diags, stream.source);
        status = 1;
    } else if (options->show_ast) {
        ast_print(unit, 0);
    }

    if (status == 0) {
        printf("\n[hcpl] parsed %s: %d declaration%s, %d token%s, %zu bytes of arena.\n",
               options->filename,
               unit->as.unit.declarations.count,
               unit->as.unit.declarations.count == 1 ? "" : "s",
               stream.count,
               stream.count == 1 ? "" : "s",
               arena_bytes_used(arena));
    }

    arena_destroy(arena);
    return status;
}

int main(int argc, char** argv) {
    Options options;
    int     i;

    options.filename    = NULL;
    options.show_tokens = 0;
    options.show_ast    = 1;

    if (argc < 3 || strcmp(argv[1], "run") != 0) {
        print_usage();
        return 1;
    }

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            options.show_tokens = 1;
        } else if (strcmp(argv[i], "--no-ast") == 0) {
            options.show_ast = 0;
        } else if (!options.filename) {
            options.filename = argv[i];
        } else {
            printf("Unknown argument: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (!options.filename) {
        print_usage();
        return 1;
    }

    return hcpl_run(&options);
}
