Param(
    [string]$Action = "run",
    [string]$File = ""
)

$Root = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $Root

$BinDir = Join-Path $Root "bin"
if (-not (Test-Path $BinDir)) { New-Item -ItemType Directory -Path $BinDir | Out-Null }

function Build {
    if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
        Write-Error "gcc not found in PATH. Install MinGW/MSYS2 and add gcc to PATH."
        return 1
    }

    $outExe = Join-Path $BinDir "hcpl.exe"

    $sources = @(
        'core/main.c',
        'core/lexer/lexer.c',
        'core/parser/parser.c',
        'core/parser/ast.c',
        'core/interpreter/interpreter.c',
        'core/runtime/runtime.c',
        'core/objects/object.c',
        'core/objects/string.c',
        'core/objects/number.c',
        'modules/math.c',
        'modules/system_io.c'
    )

    # Build by invoking gcc with an argument array to avoid quoting issues
    $args = @()
    $args += $sources
    $args += "-I$Root\include"
    $args += "-std=c11"
    $args += "-Wall"
    $args += "-Wextra"
    $args += "-o"
    $args += $outExe

    Write-Host "Building with gcc..."
    & gcc @args
    if ($LASTEXITCODE -ne 0) { Write-Error "Build failed (exit $LASTEXITCODE)"; return $LASTEXITCODE }
    Write-Host "Built: $outExe"
    return 0
}

function Run-Exe {
    $exe = Join-Path $BinDir "hcpl.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "Executable not found — building first..."
        $b = Build; if ($b -ne 0) { return $b }
    }

    if ($File -ne "") {
        Write-Host "Running: $exe run $File`n"
        & $exe run $File
    } else {
        Write-Host "Running: $exe (no file provided) `n"
        & $exe
    }

    return $LASTEXITCODE
}

switch ($Action.ToLower()) {
    'build' { exit (Build) }
    'run'   { exit (Run-Exe) }
    'clean' {
        if (Test-Path $BinDir) { Remove-Item $BinDir -Recurse -Force; Write-Host "Cleaned: $BinDir" } else { Write-Host "Nothing to clean." }
        exit 0
    }
    'rebuild' {
        if (Test-Path $BinDir) { Remove-Item $BinDir -Recurse -Force }
        exit (Build)
    }
    default {
        Write-Host "Usage: .\run.ps1 [run|build|clean|rebuild] [file]"
        exit 1
    }
}
