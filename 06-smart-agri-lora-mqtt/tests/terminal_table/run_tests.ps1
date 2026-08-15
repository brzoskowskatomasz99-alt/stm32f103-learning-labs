$ErrorActionPreference = 'Stop'

$testDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = (Resolve-Path (Join-Path $testDir '..\..')).Path
$output = Join-Path $testDir 'terminal_table_tests.exe'

try {
    & clang `
        -std=c11 `
        -Wall `
        -Wextra `
        -Werror `
        -I (Join-Path $workspace 'Core\Inc') `
        (Join-Path $testDir 'test_terminal_table.c') `
        (Join-Path $workspace 'Core\Src\terminal_table.c') `
        -o $output
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & $output
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
