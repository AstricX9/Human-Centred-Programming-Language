param(
    [string]$Action = 'run',
    [string]$FileArg = 'examples\helloworld.hpl'
)

Set-Location $PSScriptRoot

$Exe = Join-Path $PSScriptRoot 'bin\hcpl.exe'

function Build-HCPL {
    if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
        Write-Host 'Error: gcc not found in PATH. Install MinGW/MSYS2 and add gcc to PATH.'
        return 1
    }

    $binDir = Join-Path $PSScriptRoot 'bin'
    if (-not (Test-Path $binDir)) { New-Item -ItemType Directory -Path $binDir | Out-Null }

    Write-Host 'Building HCPL...'

    $sources = @(
        'core/main.c',
        'core/util/arena.c',
        'core/util/diag.c',
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

    $args = @()
    $args += $sources
    $args += "-I" + (Join-Path $PSScriptRoot 'include')
    $args += '-std=c11'
    $args += '-Wall'
    $args += '-Wextra'
    $args += '-o'
    $args += $Exe

    & gcc @args
    if ($LASTEXITCODE -ne 0) { Write-Host 'Build failed.'; return $LASTEXITCODE }

    Write-Host 'Build complete.'
    return 0
}

function Run-HCPL {
    if (-not (Test-Path $Exe)) {
        Write-Host 'hcpl.exe not found. Building first...'
        $b = Build-HCPL; if ($b -ne 0) { return $b }
    }

    Write-Host "Running: $FileArg"
    & $Exe run $FileArg
}

function Clean-HCPL {
    $bin = Join-Path $PSScriptRoot 'bin'
    if (Test-Path $bin) { Remove-Item -Recurse -Force $bin; Write-Host 'Clean complete.' } else { Write-Host 'Nothing to clean.' }
}

switch ($Action) {
    'build'   { Build-HCPL }
    'clean'   { Clean-HCPL }
    'rebuild' { Clean-HCPL; Build-HCPL }
    'run'     { Build-HCPL; Run-HCPL }
    default   { Write-Host 'Usage: .\run.ps1 [run|build|clean|rebuild] [file]' }
}
