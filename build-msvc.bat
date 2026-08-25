@echo off
REM Verification build with MSVC. The documented/portable build is run.ps1 (gcc).
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist bin mkdir bin
cl /nologo /W4 /std:c11 /D_CRT_SECURE_NO_WARNINGS /Iinclude ^
   core\main.c ^
   core\util\arena.c core\util\diag.c ^
   core\lexer\lexer.c ^
   core\parser\parser.c core\parser\ast.c ^
   /Fobin\ /Febin\hcpl.exe
endlocal
