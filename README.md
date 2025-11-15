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
hcpl/
├─ src/                # C sources (main, lexer, parser, ast, interpreter, runtime)
├─ include/            # Public/internal headers
├─ examples/           # Example .hpl programs
├─ bin/                # (created by runner) build outputs
├─ run.ps1             # PowerShell runner for build/run/clean
└─ README.md
```

Build & Run (Windows)

Prerequisites
- Install GCC (MinGW-w64) — using MSYS2 is recommended: https://www.msys2.org

Build or run using the provided PowerShell runner `run.ps1`.

From PowerShell in the project root:
```powershell
# Build only
.\run.ps1 build

# Build and run (default)
.\run.ps1 run

# Clean build artifacts
.\run.ps1 clean
```

The runner will create a `bin/` directory and place `hcpl.exe` there.

Example: run the included example
```powershell
.\run.ps1 run examples\hello.hcpl
```

Notes about GCC
- If `gcc` is not on your PATH, the runner will show an error and instructions to install/enable it.

Example HCPL snippets

Hello World
```hpl
task Hello {
	print "Hello World";
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
- Currently experimental — no licence file included yet. Add a `LICENSE` if you want to publish.
- For questions, open an issue in the repository.