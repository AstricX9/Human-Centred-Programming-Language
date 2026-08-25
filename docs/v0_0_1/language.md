*** Begin HCPL Implementation Guide (for engine developers) ***

Purpose
-------
This document is a developer-focused implementation guide for HCPL. It
assumes the visible language syntax is known and explains how to implement the
tokenizer, parser, AST, desugar phase, runtime model, module API and tests.

Goals
-----
- Deterministic parsing and predictable ASTs.
- Precise error reporting (source ranges on tokens / AST nodes).
- Simple, testable components (lexer, parser, desugar, interpreter/VM).
- A small native-module ABI for `include` bindings.

1) Tokenizer / Lexical rules
--------------------------------
Produce tokens that include type, raw text (when applicable) and source
position (line, column and byte offsets). Keep keywords as dedicated token
types and identifiers as generic lexemes.

Rules summary
- Identifiers: [A-Za-z_][A-Za-z0-9_.]* (dots allowed for namespacing; cannot start with digit)
- Numbers: integer and optional fraction (e.g. `120`, `14.5`); optional exponent if desired
- Strings: double-quoted with escape sequences `\n`, `\t`, `\\`, `\"`
- Booleans: `yes`, `no` (token BOOLEAN)
- Durations: numeric + unit suffix: `30s`, `2m`, `1h` (token DURATION)
- Keywords: `include`, `program`, `start`, `task`, `function`, `let`, `set`, `return`, `if`, `else`, `on`, `print`, etc.
- Operators/punctuation: `+ - * / { } ( ) ; , .` (tokenize each separately)

Token types (recommendation)
- EOF, IDENTIFIER, NUMBER, STRING, BOOLEAN, DURATION
- KEYWORD_* for each reserved word
- PUNCT_* and OP_* for punctuation and operators

Behavior
- Longest-match rule; if identifier text equals a keyword, return the keyword token.
- Provide helpful lexer errors (unterminated string, invalid char) with positions.

2) Parser strategy
---------------------
Prefer a recursive-descent parser with clear helper methods: `peek`, `advance`,
`match`, `expect`. Split work into two passes: parse a straightforward AST,
then run a separate desugar/normalize pass.

Why two passes
- Keep parsing rules small and clear (desugar complex natural phrases later).
- Make it easier to test normalization independently.

Parsing notes
- `and`-separated lists: implement helper `parse_and_list(parse_element_fn)`.
- Final-phrase comparisons: consume multi-token comparators (e.g. `is greater than`) and map to an internal enum.
- Expressions: because the language specifies left-to-right evaluation with no precedence by default, parse binary expressions as left-associative with equal precedence. A Pratt parser with equal precedence for all binary ops works, or write a left-associative loop.

Error recovery
- On parse error, synchronize to `;` or `}` to continue parsing and collect more errors.

3) AST shapes (recommended)
-----------------------------
Use compact, explicit node types. Example pseudo-C types:

```c
typedef struct Node { NodeKind kind; SourceRange range; } Node;

typedef struct Expr { Node base; ExprKind kind; ... } Expr;
typedef struct Stmt { Node base; StmtKind kind; ... } Stmt;

// Examples
typedef struct { Expr left; Token op; Expr right; } ExprBinary;
typedef struct { char *name; Expr **args; int argc; } ExprCall;
typedef struct { char *name; Expr *value; } StmtAssign;
typedef struct { Stmt **stmts; int count; } Block;
```

Keep AST simple – complex natural forms should be explicit nodes only until desugared.

4) Desugaring and semantic checks
----------------------------------
Desugar pass tasks:
- Transform `increase x by y` → `x = x + y` (AST Binary + assign)
- Normalize `let` / `set` forms into a single assign node
- Flatten `and`-linked arg lists into arrays
- Convert `30s` to a numeric duration node or annotate as DURATION

Semantic checks:
- Duplicate declaration detection
- Basic arity checks for native functions (optional)
- Undefined identifier checks (optionally deferred to runtime for dynamic features)

5) Runtime model
------------------
Runtime components:
- Environment stack (frames with name→Value map)
- Value type (tagged union): number, string, bool, duration, function/task, native reference
- Function/task representation: AST pointer + closure (captured env) or global scope link

Suggested C runtime API (minimal):

```c
typedef struct hcpl_value { int type; union { double number; char *string; bool boolean; } v; } hcpl_value;
typedef hcpl_value (*hcpl_native_fn)(hcpl_runtime *rt, hcpl_value *args, int argc);

typedef struct { const char *name; hcpl_native_fn fn; } hcpl_native_export;
```

Module loading
- `include foo;` should cause the runtime to register `foo.*` names from that module's export table.

6) Error handling
-------------------
All errors should include SourceRange and classification (Lexer/Parser/Semantic/Runtime).
Design errors to be easily serializable for test assertions.

7) Memory management
----------------------
- For C: use arenas for AST and token text; free whole arena after compilation to avoid many small frees.
- For runtime heap objects, use reference counting or a simple GC if necessary.

8) Testing strategy
--------------------
Create test fixtures under `tests/fixtures/`:
- `lexer/` : input → expected token arrays (JSON)
- `parser/`: input → expected AST (serialized JSON)
- `integration/`: script → expected stdout and exit status
- `errors/`: script → expected error type and source range

Example lexer fixture (JSON):

```json
[ {"type":"PRINT","text":"print","line":1,"col":1},
  {"type":"STRING","text":"Hello","line":1,"col":7},
  {"type":"SEMICOLON","text":";","line":1,"col":14},
  {"type":"EOF"} ]
```

9) Performance notes
---------------------
- Intern identifiers to reduce string allocations.
- Use an arena allocator for token and AST memory.
- Avoid reparsing strings or repeatedly copying token text during parsing.

10) Minimal implementation checklist
------------------------------------
1. Stable lexer + unit tests
2. Recursive-descent parser + unit tests
3. Desugar pass + normalization tests
4. Runtime value system + native API
5. Interpreter for basic features + integration tests

11) Actionable next docs I can produce
--------------------------------------
- `docs/dev/lexer_tests.md` with concrete token fixtures
- `docs/dev/parser_tests.md` with a few input→AST JSON fixtures
- `docs/dev/native_api.md` with C header examples for registering native functions

Tell me which of the three above you'd like next and I will create files and example fixtures.

*** End File: language.md ***
# HCPL Language Specification — v0.1

Human-Centred Programming Language (HCPL) — a compact, readable scripting language
designed to let developers write in a natural, English-like style while remaining
amenable to straightforward parsing and interpretation.

Goal: "Humans write naturally. The engine understands precisely."

## Quick Start

Example `examples/hello.hpl`:

```
include math;
include system.io;

program Main {
    start {
        print "Program started!";
    }
}
```

Run files with the project runner (see project README) or compile the engine and
execute `hcpl.exe run examples/hello.hpl`.

## Design Overview

- Syntax shape: C#/Java-style braces and semicolons for blocks/statements.
- Natural-language phrases for comparisons, assignments and argument lists.
- Simple, deterministic parsing rules: explicit keywords reduce ambiguity.

## File structure and modules

- Scripts are plain text with the `.hpl` extension.
- To import modules: `include module_name;`
- Modules expose namespaced functions accessed with dot notation:

```
math.add(10 and 20);
system.io.write("log.txt" and "Hello");
```

## Programs, entry points and tasks

- A file can contain multiple declarations: includes, programs, tasks, functions, components.
- Program example:

```
program Main {
    start {
        print "Hello";
    }
}
```

- The `start { }` block is the default entry point for a program.
- Tasks are parameterless reusable blocks (like void functions):

```
task Hello {
    print "Hello World";
}

Hello();
```

## Variables and assignment

- Assignment forms (both create variables on first use):

```
let x be 10;
set hp to 500;
set name to "Player";
```

- Read variables directly: `print hp;`.

## Functions

- Definition syntax:

```
function add(a and b) {
    return a + b;
}
```

- Parameters are listed using `and`: `ParamList := IDENTIFIER ("and" IDENTIFIER)*`.
- Calls use the same `and` linkage (no commas): `add(10 and 20);`.

## Control flow

- `if` / `else if` / `else` use English-style comparisons and no parentheses:

```
if hp is less than 0 {
    destroy();
}
else if hp is less than 50 {
    repair();
}
else {
    continue();
}
```

## Comparisons and logical operators

- Final-phrase comparisons (use exact phrases):
  - `is equal to`, `is not equal to`
  - `is greater than`, `is greater than or equal to`
  - `is less than`, `is less than or equal to`
- Logical connectors: `and`, `or` (for compound comparisons).

Example:

```
if a is greater than b and c is not equal to d {
    // ...
}
```

## Arithmetic and "natural" arithmetic

- Standard operators: `+ - * /`.
- HCPL evaluates expressions left-to-right with no operator precedence
  (so `10 + 5 * 2` → `((10 + 5) * 2)`).
- Natural arithmetic forms (syntactic sugar) map to standard operations:

```
increase hp by 20;     // hp = hp + 20
reduce armor by 10;    // armor = armor - 10
multiply chance by 1.3;// chance = chance * 1.3
divide time by 2;      // time = time / 2
```

Note: When implementing, you can desugar these into the equivalent binary operations
early in the parser or as a pre-processing step.

## Primitive types

- Strings: quoted with `"` — e.g. `"Hello World"`.
- Numbers: integers and decimals — e.g. `120`, `14.5`.
- Booleans: `yes`, `no` (map to true/false in the runtime).
- Durations: `30s`, `2m`, `1h` (parser converts to numeric seconds or a duration type).

## Events and components

- Components (objects) can declare events and properties:

```
button button_primary {
    text be "Click Me";

    on press {
        let sum be math.add(10 and 20);
        print "Sum is " + sum;
    }
}
```

- Event handler syntax: `on event_name { ... }` — no parameters in v0.1.

## Printing and returns

- Print statements: `print "Text";` or `print "Value is " + value;`.
- Return statements inside functions: `return <expr>;`.

## Comments (recommended)

Choose a single comment style for the language. Recommended (C-style):

- Single-line: `// comment`
- Block: `/* comment */`

This combination is widely supported and clear for implementers.

## Grammar summary (condensed)

The following grammar is a compact reference for parser implementers.

```
Program       := Include* Declaration*
Declaration   := ProgramDecl | TaskDecl | FuncDecl | ComponentDecl

Include       := "include" IDENTIFIER ";"

ProgramDecl   := "program" IDENTIFIER Block
TaskDecl      := "task" IDENTIFIER Block
FuncDecl      := "function" IDENTIFIER "(" ParamList? ")" Block

ParamList     := IDENTIFIER ("and" IDENTIFIER)*

Block         := "{" Statement* "}"

Statement     := VarAssign ";"
               | FuncCall ";"
               | ControlFlow
               | NaturalArithmetic ";"
               | ReturnStmt ";"
               | EventBlock

VarAssign     := ("let" IDENTIFIER "be" Expr)
               | ("set" IDENTIFIER "to" Expr)

FuncCall      := IDENTIFIER "(" ArgList? ")"
ArgList       := Expr ("and" Expr)*

ControlFlow   := IfStmt | ElseIfStmt | ElseStmt
IfStmt        := "if" Comparison Block
ElseIfStmt    := "else if" Comparison Block
ElseStmt      := "else" Block

Comparison    := Expr CompareOp Expr ( ("and"|"or") Comparison )?
CompareOp     := "is equal to" | "is not equal to"
               | "is greater than" | "is greater than or equal to"
               | "is less than"    | "is less than or equal to"

NaturalArithmetic := "increase" IDENTIFIER "by" Expr
                   | "reduce" IDENTIFIER "by" Expr
                   | "multiply" IDENTIFIER "by" Expr
                   | "divide" IDENTIFIER "by" Expr

ReturnStmt    := "return" Expr
EventBlock    := "on" IDENTIFIER Block

Expr          := Term ( ("+"|"-") Term )*
Term          := Factor ( ("*"|"/") Factor )*
Factor        := NUMBER | STRING | BOOLEAN | DURATION
               | IDENTIFIER | FuncCall
```

## Engine integration notes (implementation checklist)

- Tokenizer: returns tokens (identifiers may include dots for namespacing).
- Parser: produce AST nodes for declarations, statements and expressions.
- Interpreter/VM: evaluate AST or compile to bytecode.
- Native bindings: expose runtime functions (print, I/O, math, GUI).
- Module loader and runtime context: manage variables, functions, tasks and events.

Suggested immediate implementation steps:

1. Tokenizer (lexing) with clear token types for identifiers, keywords, literals, punctuation.
2. Parser producing an AST matching the grammar above.
3. Interpreter executing programs with a runtime context (scopes, native bindings).
4. Native module interface for `include`-able modules.

## Examples

Function and call:

```
function clamp(value and min and max) {
    if value is less than min {
        return min;
    }
    else if value is greater than max {
        return max;
    }
    return value;
}

clamp(5 and 0 and 10);
```

Natural arithmetic:

```
increase hp by 20;
```

Comparison example:

```
if a is greater than b and c is not equal to d {
    print "conditions met";
}
```

---

Changelog: this document is v0.1 — keep a small changelog in the repo to track future syntax changes.
