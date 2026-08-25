#ifndef HCPL_AST_H
#define HCPL_AST_H

/*
 * HCPL abstract syntax tree.
 *
 * Deliberately follows docs/v0_0_1/language.md section 3: one compact Node
 * type with a kind tag and a source position, and natural-language forms kept
 * as explicit nodes rather than desugared during parsing. NODE_NATURAL is the
 * clearest example -- `increase hp by 20` stays as itself here and only
 * becomes an assignment in the desugar pass, so parsing and normalisation can
 * be tested apart from one another.
 *
 * Everything is arena-allocated. There is no ast_free(); destroy the arena.
 */

#include "arena.h"

typedef struct Node Node;

typedef enum {
    /* expressions */
    NODE_NUMBER,
    NODE_STRING,
    NODE_BOOLEAN,
    NODE_DURATION,
    NODE_IDENTIFIER,
    NODE_UNARY,
    NODE_BINARY,
    NODE_COMPARISON,
    NODE_LOGICAL,
    NODE_CALL,

    /* statements */
    NODE_LET,
    NODE_SET,
    NODE_PROPERTY,     /* `text be "Click Me";` inside a component */
    NODE_PRINT,
    NODE_RETURN,
    NODE_EXPR_STMT,
    NODE_IF,
    NODE_NATURAL,
    NODE_EVENT,        /* `on press { ... }` */
    NODE_BLOCK,        /* the body of a bare `else` */

    /* declarations */
    NODE_INCLUDE,
    NODE_PROGRAM,
    NODE_START,
    NODE_TASK,
    NODE_FUNCTION,
    NODE_COMPONENT,    /* `button name { ... }` */

    /* root */
    NODE_UNIT,

    NODE_KIND_COUNT
} NodeKind;

typedef enum {
    BINARY_ADD,
    BINARY_SUBTRACT,
    BINARY_MULTIPLY,
    BINARY_DIVIDE
} BinaryOp;

typedef enum {
    COMPARE_EQUAL,
    COMPARE_NOT_EQUAL,
    COMPARE_GREATER,
    COMPARE_GREATER_OR_EQUAL,
    COMPARE_LESS,
    COMPARE_LESS_OR_EQUAL
} CompareOp;

typedef enum {
    NATURAL_INCREASE,
    NATURAL_REDUCE,
    NATURAL_MULTIPLY,
    NATURAL_DIVIDE
} NaturalOp;

/* Grows inside the arena by allocating a larger array and abandoning the old
   one. See arena.h for why that is acceptable here. */
typedef struct {
    Node** items;
    int    count;
    int    capacity;
} NodeList;

struct Node {
    NodeKind kind;
    int      line;
    int      column;

    union {
        struct { double value; }                                    number;
        struct { char* value; }                                     string;
        struct { int value; }                                       boolean;   /* yes = 1 */
        struct { double seconds; char* raw; }                       duration;
        struct { char* name; }                                      identifier;

        struct { Node* operand; }                                   unary;     /* negation only */
        struct { BinaryOp op;  Node* left; Node* right; }           binary;
        struct { CompareOp op; Node* left; Node* right; }           comparison;
        struct { int is_or;    Node* left; Node* right; }           logical;
        struct { char* name;   NodeList args; }                     call;

        struct { char* name; Node* value; }                         assign;    /* let/set/property */
        struct { Node* value; }                                     single;    /* print/return/expr */
        struct { Node* condition; NodeList body; Node* otherwise; } if_stmt;
        struct { NaturalOp op; char* target; Node* amount; }        natural;

        struct { char* name; }                                      include;
        struct { char* name; NodeList body; }                       block;     /* program/task/start/event */
        struct { char* name; NodeList params; NodeList body; }      function;
        struct { char* type_name; char* name; NodeList body; }      component;
        struct { NodeList declarations; }                           unit;
    } as;
};

/* ------------------------------------------------------------ construction */

Node* ast_node(Arena* arena, NodeKind kind, int line, int column);

void  node_list_init(NodeList* list);
void  node_list_push(Arena* arena, NodeList* list, Node* node);

/* ---------------------------------------------------------------- printing */

/* Indented tree dump. This is the only way to see parser output until the
   back end exists, so it prints every field the parser fills in. */
void ast_print(const Node* node, int indent);

const char* node_kind_name(NodeKind kind);
const char* binary_op_symbol(BinaryOp op);
const char* compare_op_phrase(CompareOp op);
const char* natural_op_name(NaturalOp op);

/* Converts a duration lexeme such as "30s" or "2m" into seconds. */
double duration_to_seconds(const char* text);

#endif /* HCPL_AST_H */
