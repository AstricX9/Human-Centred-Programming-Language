#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------ construction */

Node* ast_node(Arena* arena, NodeKind kind, int line, int column) {
    Node* node = (Node*)arena_zalloc(arena, sizeof(Node));
    if (!node) return NULL;

    node->kind   = kind;
    node->line   = line;
    node->column = column;
    return node;
}

void node_list_init(NodeList* list) {
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void node_list_push(Arena* arena, NodeList* list, Node* node) {
    if (!node) return;

    if (list->count >= list->capacity) {
        int    grown = list->capacity ? list->capacity * 2 : 4;
        Node** moved = (Node**)arena_alloc(arena, sizeof(Node*) * (size_t)grown);
        if (!moved) return;

        if (list->count) memcpy(moved, list->items, sizeof(Node*) * (size_t)list->count);
        list->items    = moved;
        list->capacity = grown;
    }

    list->items[list->count++] = node;
}

/* ------------------------------------------------------------------- names */

static const char* NODE_KIND_NAMES[NODE_KIND_COUNT] = {
    "Number", "String", "Boolean", "Duration", "Identifier",
    "Unary", "Binary", "Comparison", "Logical", "Call",
    "Let", "Set", "Property", "Print", "Return", "ExprStmt",
    "If", "Natural", "Event", "Block",
    "Include", "Program", "Start", "Task", "Function", "Component",
    "Unit"
};

const char* node_kind_name(NodeKind kind) {
    if (kind < 0 || kind >= NODE_KIND_COUNT) return "?";
    return NODE_KIND_NAMES[kind];
}

const char* binary_op_symbol(BinaryOp op) {
    switch (op) {
        case BINARY_ADD:      return "+";
        case BINARY_SUBTRACT: return "-";
        case BINARY_MULTIPLY: return "*";
        case BINARY_DIVIDE:   return "/";
        default:              return "?";
    }
}

const char* compare_op_phrase(CompareOp op) {
    switch (op) {
        case COMPARE_EQUAL:            return "is equal to";
        case COMPARE_NOT_EQUAL:        return "is not equal to";
        case COMPARE_GREATER:          return "is greater than";
        case COMPARE_GREATER_OR_EQUAL: return "is greater than or equal to";
        case COMPARE_LESS:             return "is less than";
        case COMPARE_LESS_OR_EQUAL:    return "is less than or equal to";
        default:                       return "?";
    }
}

const char* natural_op_name(NaturalOp op) {
    switch (op) {
        case NATURAL_INCREASE: return "increase";
        case NATURAL_REDUCE:   return "reduce";
        case NATURAL_MULTIPLY: return "multiply";
        case NATURAL_DIVIDE:   return "divide";
        default:               return "?";
    }
}

double duration_to_seconds(const char* text) {
    char*  end   = NULL;
    double value;

    if (!text) return 0.0;

    value = strtod(text, &end);
    if (!end || !*end) return value;

    switch (*end) {
        case 's': return value;
        case 'm': return value * 60.0;
        case 'h': return value * 3600.0;
        default:  return value;
    }
}

/* ---------------------------------------------------------------- printing */

static void print_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++) fputs("  ", stdout);
}

static void print_list(const char* label, const NodeList* list, int indent) {
    int i;

    if (list->count == 0) return;

    print_indent(indent);
    printf("%s:\n", label);

    for (i = 0; i < list->count; i++) {
        ast_print(list->items[i], indent + 1);
    }
}

void ast_print(const Node* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("<null>\n");
        return;
    }

    print_indent(indent);
    printf("%s", node_kind_name(node->kind));

    switch (node->kind) {
        case NODE_NUMBER:
            printf(" %g\n", node->as.number.value);
            break;

        case NODE_STRING:
            printf(" \"%s\"\n", node->as.string.value ? node->as.string.value : "");
            break;

        case NODE_BOOLEAN:
            printf(" %s\n", node->as.boolean.value ? "yes" : "no");
            break;

        case NODE_DURATION:
            printf(" %s (%g seconds)\n",
                   node->as.duration.raw ? node->as.duration.raw : "",
                   node->as.duration.seconds);
            break;

        case NODE_IDENTIFIER:
            printf(" %s\n", node->as.identifier.name);
            break;

        case NODE_UNARY:
            printf(" negate\n");
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case NODE_BINARY:
            printf(" %s\n", binary_op_symbol(node->as.binary.op));
            ast_print(node->as.binary.left,  indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;

        case NODE_COMPARISON:
            printf(" %s\n", compare_op_phrase(node->as.comparison.op));
            ast_print(node->as.comparison.left,  indent + 1);
            ast_print(node->as.comparison.right, indent + 1);
            break;

        case NODE_LOGICAL:
            printf(" %s\n", node->as.logical.is_or ? "or" : "and");
            ast_print(node->as.logical.left,  indent + 1);
            ast_print(node->as.logical.right, indent + 1);
            break;

        case NODE_CALL:
            printf(" %s (%d argument%s)\n",
                   node->as.call.name,
                   node->as.call.args.count,
                   node->as.call.args.count == 1 ? "" : "s");
            print_list("args", &node->as.call.args, indent + 1);
            break;

        case NODE_LET:
        case NODE_SET:
        case NODE_PROPERTY:
            printf(" %s\n", node->as.assign.name);
            ast_print(node->as.assign.value, indent + 1);
            break;

        case NODE_PRINT:
        case NODE_RETURN:
        case NODE_EXPR_STMT:
            printf("\n");
            if (node->as.single.value) ast_print(node->as.single.value, indent + 1);
            break;

        case NODE_IF:
            printf("\n");
            print_indent(indent + 1);
            printf("condition:\n");
            ast_print(node->as.if_stmt.condition, indent + 2);
            print_list("then", &node->as.if_stmt.body, indent + 1);
            if (node->as.if_stmt.otherwise) {
                print_indent(indent + 1);
                printf("else:\n");
                ast_print(node->as.if_stmt.otherwise, indent + 2);
            }
            break;

        case NODE_NATURAL:
            printf(" %s %s by\n",
                   natural_op_name(node->as.natural.op),
                   node->as.natural.target);
            ast_print(node->as.natural.amount, indent + 1);
            break;

        case NODE_INCLUDE:
            printf(" %s\n", node->as.include.name);
            break;

        case NODE_EVENT:
        case NODE_BLOCK:
        case NODE_PROGRAM:
        case NODE_START:
        case NODE_TASK:
            printf("%s%s\n",
                   node->as.block.name ? " " : "",
                   node->as.block.name ? node->as.block.name : "");
            print_list("body", &node->as.block.body, indent + 1);
            break;

        case NODE_FUNCTION:
            printf(" %s\n", node->as.function.name);
            print_list("params", &node->as.function.params, indent + 1);
            print_list("body",   &node->as.function.body,   indent + 1);
            break;

        case NODE_COMPONENT:
            printf(" %s %s\n", node->as.component.type_name, node->as.component.name);
            print_list("body", &node->as.component.body, indent + 1);
            break;

        case NODE_UNIT:
            printf(" (%d declaration%s)\n",
                   node->as.unit.declarations.count,
                   node->as.unit.declarations.count == 1 ? "" : "s");
            print_list("declarations", &node->as.unit.declarations, indent + 1);
            break;

        default:
            printf("\n");
            break;
    }
}
