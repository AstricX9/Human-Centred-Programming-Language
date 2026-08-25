#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Token* tokens;
    int          count;
    int          index;

    Arena*       arena;
    DiagList*    diags;

    /* Set when an error is reported, cleared by synchronise(). Suppresses the
       cascade of follow-on errors a single mistake would otherwise produce. */
    int          panic;
} Parser;

static Node* parse_statement(Parser* p);
static Node* parse_expression(Parser* p);
static Node* parse_condition(Parser* p);
static Node* parse_component(Parser* p);
static Node* parse_task(Parser* p);
static Node* parse_function(Parser* p);
static Node* parse_if(Parser* p);
static int   parse_block_body(Parser* p, NodeList* body);

/* --------------------------------------------------------------- cursor */

static const Token* peek(Parser* p) {
    return &p->tokens[p->index];
}

static const Token* peek_ahead(Parser* p, int ahead) {
    int index = p->index + ahead;
    if (index >= p->count) index = p->count - 1;   /* clamps to EOF */
    return &p->tokens[index];
}

static int at_end(Parser* p) {
    return peek(p)->type == TOKEN_EOF;
}

static int check(Parser* p, TokenType type) {
    return peek(p)->type == type;
}

static const Token* advance(Parser* p) {
    const Token* token = peek(p);
    if (!at_end(p)) p->index++;
    return token;
}

static int match(Parser* p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

/* ---------------------------------------------------------------- errors */

static void parser_error(Parser* p, const Token* token, const char* format, ...) {
    va_list args;
    char*   message;

    if (p->panic) return;
    p->panic = 1;

    va_start(args, format);
    message = arena_vsprintf(p->arena, format, args);
    va_end(args);

    diag_add(p->diags, DIAG_PARSER, token->line, token->column, token->length,
             "%s", message ? message : "parse error");
}

/* Skips forward until something that plausibly starts a new statement or
   declaration, so one bad line does not poison the rest of the file. */
static void synchronise(Parser* p) {
    p->panic = 0;

    while (!at_end(p)) {
        if (p->index > 0 && p->tokens[p->index - 1].type == TOKEN_SEMICOLON) return;

        switch (peek(p)->type) {
            case TOKEN_RBRACE:
            case TOKEN_INCLUDE:
            case TOKEN_PROGRAM:
            case TOKEN_TASK:
            case TOKEN_FUNCTION:
            case TOKEN_BUTTON:
            case TOKEN_START:
            case TOKEN_ON:
            case TOKEN_LET:
            case TOKEN_SET:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
            case TOKEN_IF:
            case TOKEN_INCREASE:
            case TOKEN_REDUCE:
            case TOKEN_MULTIPLY:
            case TOKEN_DIVIDE:
                return;
            default:
                advance(p);
                break;
        }
    }
}

static const Token* expect(Parser* p, TokenType type, const char* description) {
    if (check(p, type)) return advance(p);

    parser_error(p, peek(p), "expected %s but found '%s'",
                 description, peek(p)->text ? peek(p)->text : "?");
    return NULL;
}

static char* expect_name(Parser* p, const char* description) {
    const Token* token = expect(p, TOKEN_IDENTIFIER, description);
    if (token) return token->text;

    /* Step over the offending token unless it is structural. Leaving it in
       place made `program 42 { ... }` report five errors: the name failed, the
       brace check then failed on the same token, and the whole body was
       re-read as top-level statements. Consuming it costs one error instead. */
    switch (peek(p)->type) {
        case TOKEN_LBRACE:
        case TOKEN_RBRACE:
        case TOKEN_SEMICOLON:
        case TOKEN_EOF:
            break;
        default:
            advance(p);
            break;
    }
    return NULL;
}

/* Panic mode suppresses follow-on errors inside one broken statement. It has
   to be cleared at every statement boundary, otherwise the first error in a
   file silences every later one -- most parse functions here return a node
   even after failing, so synchronise() alone never runs. */
static void begin_statement(Parser* p) {
    p->panic = 0;
}

/* --------------------------------------------------------- expressions */

static Node* parse_call(Parser* p, const Token* name_token) {
    Node* call = ast_node(p->arena, NODE_CALL, name_token->line, name_token->column);

    call->as.call.name = name_token->text;
    node_list_init(&call->as.call.args);

    expect(p, TOKEN_LPAREN, "'(' after a function name");

    if (!check(p, TOKEN_RPAREN) && !at_end(p)) {
        do {
            Node* argument = parse_expression(p);
            if (!argument) break;
            node_list_push(p->arena, &call->as.call.args, argument);
            /* The spec links arguments with `and`; concepts/task.hpl uses
               commas. Both are accepted until one of them wins. */
        } while (match(p, TOKEN_AND) || match(p, TOKEN_COMMA));
    }

    expect(p, TOKEN_RPAREN, "')' to close the argument list");
    return call;
}

static Node* parse_primary(Parser* p) {
    const Token* token = peek(p);
    Node*        node;

    switch (token->type) {
        case TOKEN_NUMBER:
            advance(p);
            node = ast_node(p->arena, NODE_NUMBER, token->line, token->column);
            node->as.number.value = strtod(token->text, NULL);
            return node;

        case TOKEN_STRING:
            advance(p);
            node = ast_node(p->arena, NODE_STRING, token->line, token->column);
            node->as.string.value = token->text;
            return node;

        case TOKEN_BOOLEAN:
            advance(p);
            node = ast_node(p->arena, NODE_BOOLEAN, token->line, token->column);
            node->as.boolean.value = (strcmp(token->text, "yes") == 0);
            return node;

        case TOKEN_DURATION:
            advance(p);
            node = ast_node(p->arena, NODE_DURATION, token->line, token->column);
            node->as.duration.raw     = token->text;
            node->as.duration.seconds = duration_to_seconds(token->text);
            return node;

        case TOKEN_IDENTIFIER:
            advance(p);
            if (check(p, TOKEN_LPAREN)) return parse_call(p, token);

            node = ast_node(p->arena, NODE_IDENTIFIER, token->line, token->column);
            node->as.identifier.name = token->text;
            return node;

        case TOKEN_LPAREN:
            advance(p);
            node = parse_expression(p);
            expect(p, TOKEN_RPAREN, "')' to close the group");
            return node;

        default:
            parser_error(p, token, "expected a value but found '%s'",
                         token->text ? token->text : "?");
            return NULL;
    }
}

/* Negation is not in the v0.1 grammar, but rejecting `-5` outright produces a
   baffling error for something every writer will try. */
static Node* parse_unary(Parser* p) {
    if (check(p, TOKEN_MINUS)) {
        const Token* op   = advance(p);
        Node*        node = ast_node(p->arena, NODE_UNARY, op->line, op->column);

        node->as.unary.operand = parse_unary(p);
        return node;
    }
    return parse_primary(p);
}

static int binary_op_for(TokenType type, BinaryOp* out) {
    switch (type) {
        case TOKEN_PLUS:  *out = BINARY_ADD;      return 1;
        case TOKEN_MINUS: *out = BINARY_SUBTRACT; return 1;
        case TOKEN_STAR:  *out = BINARY_MULTIPLY; return 1;
        case TOKEN_SLASH: *out = BINARY_DIVIDE;   return 1;
        default:                                  return 0;
    }
}

/*
 * One flat left-associative loop, on purpose.
 *
 * language.md states twice that HCPL has no operator precedence and evaluates
 * left to right, so `10 + 5 * 2` is `((10 + 5) * 2)` = 30. The condensed
 * grammar at the bottom of that document contradicts this with a conventional
 * Expr/Term split; the prose wins here because it is stated normatively and
 * repeated. See the note added to docs/v0_0_1/language.md.
 *
 * Note that `and` is never consumed here. It appears only in argument lists,
 * parameter lists and between comparisons, so its role is always decided by
 * the caller rather than by precedence.
 */
static Node* parse_expression(Parser* p) {
    Node*    left = parse_unary(p);
    BinaryOp op;

    if (!left) return NULL;

    while (binary_op_for(peek(p)->type, &op)) {
        const Token* op_token = advance(p);
        Node*        right    = parse_unary(p);
        Node*        combined;

        if (!right) return left;

        combined = ast_node(p->arena, NODE_BINARY, op_token->line, op_token->column);
        combined->as.binary.op    = op;
        combined->as.binary.left  = left;
        combined->as.binary.right = right;
        left = combined;
    }

    return left;
}

/*
 * Consumes a final-phrase comparator.
 *
 * The tricky case is `is greater than or equal to`, because `or` is also the
 * logical connector: `a is greater than or equal to b` and
 * `a is greater than b or c is less than d` diverge three tokens after `than`.
 * Since the lexer hands us the whole token array, the three-token lookahead
 * that resolves it is just an index check.
 */
static int parse_compare_op(Parser* p, CompareOp* out) {
    int negated;

    expect(p, TOKEN_IS, "'is' to begin a comparison");
    negated = match(p, TOKEN_NOT);

    if (match(p, TOKEN_EQUAL)) {
        expect(p, TOKEN_TO, "'to' after 'equal'");
        *out = negated ? COMPARE_NOT_EQUAL : COMPARE_EQUAL;
        return 1;
    }

    if (negated) {
        parser_error(p, peek(p),
                     "'is not' is only supported as 'is not equal to'");
        return 0;
    }

    if (match(p, TOKEN_GREATER) || match(p, TOKEN_LESS)) {
        int is_less = (p->tokens[p->index - 1].type == TOKEN_LESS);

        expect(p, TOKEN_THAN, "'than' after 'greater' or 'less'");

        if (check(p, TOKEN_OR) &&
            peek_ahead(p, 1)->type == TOKEN_EQUAL &&
            peek_ahead(p, 2)->type == TOKEN_TO) {
            advance(p);
            advance(p);
            advance(p);
            *out = is_less ? COMPARE_LESS_OR_EQUAL : COMPARE_GREATER_OR_EQUAL;
            return 1;
        }

        *out = is_less ? COMPARE_LESS : COMPARE_GREATER;
        return 1;
    }

    parser_error(p, peek(p),
                 "expected 'equal to', 'greater than' or 'less than' after 'is'");
    return 0;
}

/* A bare expression is accepted as a condition. The v0.1 grammar requires a
   comparator, but allowing `if flag { }` costs nothing and keeps the error for
   genuinely malformed conditions pointed at the right token. */
static Node* parse_comparison(Parser* p) {
    Node*     left = parse_expression(p);
    CompareOp op;
    Node*     node;

    if (!left) return NULL;
    if (!check(p, TOKEN_IS)) return left;

    if (!parse_compare_op(p, &op)) return left;

    node = ast_node(p->arena, NODE_COMPARISON, left->line, left->column);
    node->as.comparison.op    = op;
    node->as.comparison.left  = left;
    node->as.comparison.right = parse_expression(p);
    return node;
}

static Node* parse_condition(Parser* p) {
    Node* left = parse_comparison(p);

    if (!left) return NULL;

    while (check(p, TOKEN_AND) || check(p, TOKEN_OR)) {
        const Token* connector = advance(p);
        Node*        right     = parse_comparison(p);
        Node*        combined;

        if (!right) return left;

        combined = ast_node(p->arena, NODE_LOGICAL, connector->line, connector->column);
        combined->as.logical.is_or = (connector->type == TOKEN_OR);
        combined->as.logical.left  = left;
        combined->as.logical.right = right;
        left = combined;
    }

    return left;
}

/* ---------------------------------------------------------- statements */

static Node* parse_assignment(Parser* p, NodeKind kind, TokenType linking_word,
                              const char* linking_description) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, kind, keyword->line, keyword->column);

    node->as.assign.name = expect_name(p, "a variable name");
    expect(p, linking_word, linking_description);
    node->as.assign.value = parse_expression(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    return node;
}

static Node* parse_natural(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_NATURAL, keyword->line, keyword->column);

    switch (keyword->type) {
        case TOKEN_INCREASE: node->as.natural.op = NATURAL_INCREASE; break;
        case TOKEN_REDUCE:   node->as.natural.op = NATURAL_REDUCE;   break;
        case TOKEN_MULTIPLY: node->as.natural.op = NATURAL_MULTIPLY; break;
        default:             node->as.natural.op = NATURAL_DIVIDE;   break;
    }

    node->as.natural.target = expect_name(p, "a variable name");
    expect(p, TOKEN_BY, "'by'");
    node->as.natural.amount = parse_expression(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    return node;
}

static Node* parse_event(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_EVENT, keyword->line, keyword->column);

    /* The v0.1 grammar is `on IDENTIFIER Block`, so the event name is an
       ordinary identifier rather than a reserved word. `press` is not a
       keyword; `on hover` works without a lexer change. */
    node->as.block.name = expect_name(p, "an event name after 'on'");
    node_list_init(&node->as.block.body);
    parse_block_body(p, &node->as.block.body);
    return node;
}

static Node* parse_if(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_IF, keyword->line, keyword->column);

    node->as.if_stmt.condition = parse_condition(p);
    node_list_init(&node->as.if_stmt.body);
    parse_block_body(p, &node->as.if_stmt.body);

    if (match(p, TOKEN_ELSE)) {
        if (check(p, TOKEN_IF)) {
            node->as.if_stmt.otherwise = parse_if(p);
        } else {
            const Token* brace = peek(p);
            Node*        block = ast_node(p->arena, NODE_BLOCK, brace->line, brace->column);

            block->as.block.name = NULL;
            node_list_init(&block->as.block.body);
            parse_block_body(p, &block->as.block.body);
            node->as.if_stmt.otherwise = block;
        }
    }

    return node;
}

static Node* parse_statement(Parser* p) {
    const Token* token = peek(p);
    Node*        node;

    switch (token->type) {
        case TOKEN_LET:
            return parse_assignment(p, NODE_LET, TOKEN_BE, "'be'");

        case TOKEN_SET:
            return parse_assignment(p, NODE_SET, TOKEN_TO, "'to'");

        case TOKEN_PRINT:
            advance(p);
            node = ast_node(p->arena, NODE_PRINT, token->line, token->column);
            node->as.single.value = parse_expression(p);
            expect(p, TOKEN_SEMICOLON, "';'");
            return node;

        case TOKEN_RETURN:
            advance(p);
            node = ast_node(p->arena, NODE_RETURN, token->line, token->column);
            if (!check(p, TOKEN_SEMICOLON)) node->as.single.value = parse_expression(p);
            expect(p, TOKEN_SEMICOLON, "';'");
            return node;

        case TOKEN_IF:
            return parse_if(p);

        case TOKEN_INCREASE:
        case TOKEN_REDUCE:
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
            return parse_natural(p);

        case TOKEN_ON:
            return parse_event(p);

        case TOKEN_BUTTON:
            return parse_component(p);

        case TOKEN_TASK:
            return parse_task(p);

        case TOKEN_FUNCTION:
            return parse_function(p);

        case TOKEN_IDENTIFIER:
            /* `text be "Click Me";` is a property assignment; anything else
               starting with a name is an expression statement, in practice a
               call. One token of lookahead separates them.

               This cannot reuse parse_assignment: there the statement opens
               with a keyword (`let`/`set`) and the name follows, whereas a
               property opens with the name itself. */
            if (peek_ahead(p, 1)->type == TOKEN_BE) {
                node = ast_node(p->arena, NODE_PROPERTY, token->line, token->column);
                node->as.assign.name = advance(p)->text;
                advance(p);   /* `be` */
                node->as.assign.value = parse_expression(p);
                expect(p, TOKEN_SEMICOLON, "';'");
                return node;
            }

            node = ast_node(p->arena, NODE_EXPR_STMT, token->line, token->column);
            node->as.single.value = parse_expression(p);
            expect(p, TOKEN_SEMICOLON, "';'");
            return node;

        default:
            parser_error(p, token, "expected a statement but found '%s'",
                         token->text ? token->text : "?");
            return NULL;
    }
}

/* Parses `{ statement* }`. Returns 1 if the braces were found. The
   index-unchanged guard is what stops a statement that consumes nothing from
   spinning this loop forever. */
static int parse_block_body(Parser* p, NodeList* body) {
    if (!expect(p, TOKEN_LBRACE, "'{'")) {
        synchronise(p);
        return 0;
    }

    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        int   before = p->index;
        Node* statement;

        begin_statement(p);
        statement = parse_statement(p);

        if (statement) {
            node_list_push(p->arena, body, statement);
        } else {
            synchronise(p);
        }

        if (p->index == before) advance(p);
    }

    expect(p, TOKEN_RBRACE, "'}' to close the block");
    return 1;
}

/* -------------------------------------------------------- declarations */

static Node* parse_include(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_INCLUDE, keyword->line, keyword->column);

    node->as.include.name = expect_name(p, "a module name after 'include'");
    expect(p, TOKEN_SEMICOLON, "';'");
    return node;
}

static Node* parse_task(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_TASK, keyword->line, keyword->column);

    node->as.block.name = expect_name(p, "a task name");
    node_list_init(&node->as.block.body);
    parse_block_body(p, &node->as.block.body);
    return node;
}

static Node* parse_function(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_FUNCTION, keyword->line, keyword->column);

    node->as.function.name = expect_name(p, "a function name");
    node_list_init(&node->as.function.params);
    node_list_init(&node->as.function.body);

    expect(p, TOKEN_LPAREN, "'(' after a function name");

    if (!check(p, TOKEN_RPAREN) && !at_end(p)) {
        do {
            const Token* param = expect(p, TOKEN_IDENTIFIER, "a parameter name");
            Node*        node_param;

            if (!param) break;

            node_param = ast_node(p->arena, NODE_IDENTIFIER, param->line, param->column);
            node_param->as.identifier.name = param->text;
            node_list_push(p->arena, &node->as.function.params, node_param);
        } while (match(p, TOKEN_AND) || match(p, TOKEN_COMMA));
    }

    expect(p, TOKEN_RPAREN, "')' to close the parameter list");
    parse_block_body(p, &node->as.function.body);
    return node;
}

static Node* parse_component(Parser* p) {
    const Token* keyword = advance(p);   /* `button`, for now the only kind */
    Node*        node    = ast_node(p->arena, NODE_COMPONENT, keyword->line, keyword->column);

    node->as.component.type_name = keyword->text;
    node->as.component.name      = expect_name(p, "a component name");
    node_list_init(&node->as.component.body);
    parse_block_body(p, &node->as.component.body);
    return node;
}

static Node* parse_start(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_START, keyword->line, keyword->column);

    node->as.block.name = NULL;
    node_list_init(&node->as.block.body);
    parse_block_body(p, &node->as.block.body);
    return node;
}

static Node* parse_program(Parser* p) {
    const Token* keyword = advance(p);
    Node*        node    = ast_node(p->arena, NODE_PROGRAM, keyword->line, keyword->column);

    node->as.block.name = expect_name(p, "a program name");
    node_list_init(&node->as.block.body);

    if (!expect(p, TOKEN_LBRACE, "'{' after the program name")) {
        synchronise(p);
        return node;
    }

    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        int   before = p->index;
        Node* member;

        begin_statement(p);

        switch (peek(p)->type) {
            case TOKEN_START: member = parse_start(p);     break;
            default:          member = parse_statement(p); break;
        }

        if (member) {
            node_list_push(p->arena, &node->as.block.body, member);
        } else {
            synchronise(p);
        }

        if (p->index == before) advance(p);
    }

    expect(p, TOKEN_RBRACE, "'}' to close the program");
    return node;
}

static Node* parse_declaration(Parser* p) {
    switch (peek(p)->type) {
        case TOKEN_INCLUDE:  return parse_include(p);
        case TOKEN_PROGRAM:  return parse_program(p);
        case TOKEN_TASK:     return parse_task(p);
        case TOKEN_FUNCTION: return parse_function(p);
        case TOKEN_BUTTON:   return parse_component(p);
        default:
            /* The spec's own examples call tasks at file scope, so a bare
               statement is a legal top-level form. */
            return parse_statement(p);
    }
}

Node* parse_unit(Arena* arena, const TokenStream* stream, DiagList* diags) {
    Parser p;
    Node*  unit;

    p.tokens = stream->tokens;
    p.count  = stream->count;
    p.index  = 0;
    p.arena  = arena;
    p.diags  = diags;
    p.panic  = 0;

    unit = ast_node(arena, NODE_UNIT, 1, 1);
    node_list_init(&unit->as.unit.declarations);

    while (!at_end(&p)) {
        int   before = p.index;
        Node* declaration;

        begin_statement(&p);
        declaration = parse_declaration(&p);

        if (declaration) {
            node_list_push(arena, &unit->as.unit.declarations, declaration);
        } else {
            synchronise(&p);
        }

        if (p.index == before) advance(&p);
    }

    return unit;
}
