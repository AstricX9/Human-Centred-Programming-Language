@echo off
REM Portable gcc build. See run.ps1 for the fuller build/run/clean workflow.
if not exist bin mkdir bin
gcc core\main.c ^
    core\util\arena.c core\util\diag.c ^
    core\lexer\lexer.c ^
    core\parser\parser.c core\parser\ast.c ^
    core\interpreter\interpreter.c ^
    core\runtime\runtime.c ^
    core\objects\object.c core\objects\string.c core\objects\number.c ^
    modules\math.c modules\system_io.c ^
    -Iinclude -std=c11 -Wall -Wextra -o bin\hcpl.exe
echo Build complete.
