# HCPL — Human-Centered Programming Language

HCPL (Human-Centered Programming Language) is an experimental language that prioritises human readability and intent over machine-oriented syntax. The compiler and runtime are written in C and use a small custom runner so you don't need heavyweight build tools.

Highlights
 - Human-friendly syntax with familiar C-style structure
 - Modular compiler pipeline: Lexer → Parser → AST → Interpreter → Runtime
 - Simple custom runner for build/run on Windows

Quick Example
```hpl
task Hello {
	print "Hello World";
}
```

File Extension
- Use `.hpl` for HCPL source files.

Project Structure
```
HCPL/
├─ core/                # Core language implementation (C sources)
│  ├─ lexer/
│  ├─ parser/
│  ├─ interpreter/
│  ├─ runtime/
│  └─ main.c            # launcher
├─ include/             # Public/internal headers (canonical headers live here)
├─ modules/             # Built-in native C modules
├─ lib/                 # Standard library written in HCPL
├─ examples/            # Example .hpl programs
├─ bin/                 # (created by build) build outputs
└─ README.md
```

Build & Run (Windows)

Prerequisites
- Install GCC (MinGW-w64) — using MSYS2 is recommended: https://www.msys2.org

Build (recommended quick command)

From PowerShell or CMD in the project root you can compile the core sources directly:
```powershell
gcc core/main.c \
	core/lexer/lexer.c \
	core/parser/parser.c core/parser/ast.c \
	core/interpreter/interpreter.c \
	core/runtime/runtime.c \
	core/objects/object.c core/objects/string.c core/objects/number.c \
	modules/math.c modules/system_io.c \
	-Iinclude -std=c11 -Wall -Wextra -o hcpl.exe
```

This produces `hcpl.exe` in the project root. Example run:
```powershell
.\hcpl.exe run examples\helloworld.hpl
```

Notes about GCC
- If `gcc` is not on your PATH, install it (MSYS2/MinGW-w64) and add the MinGW `bin` directory to PATH.

Example HCPL snippets

Hello World Application
```hpl
program Main {
    start {
        print "Hello World";
    }
}

```

Variables & Expressions
```hpl
task Math {
	let x be 10;
	let y = x * 5 + 3;
	print y;
}
```

Automation (concept)
```hpl
task Backup {
	every 24h {
		copy "Projects" to "Backups";
	}
}
```

Future Plans
- Complete lexer, parser and formal grammar
- AST builder and interpreter improvements
- Optional bytecode compiler and VM
- Tooling: package manager, transpiler, self-hosting compiler

Contributing
- HCPL is an early-stage experimental project. Contributions, ideas and issues are welcome. Please open issues or PRs with small, focused changes.

License & Contact
- Currently experimental — no licence included yet.
- For questions, open an issue in the repository.