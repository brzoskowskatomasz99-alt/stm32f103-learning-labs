$ErrorActionPreference = 'Stop'

$testDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = (Resolve-Path (Join-Path $testDir '..\..')).Path
$output = Join-Path $testDir 'terminal_autonomy_tests.exe'

try {
    & clang `
        -std=c11 `
        -Wall `
        -Wextra `
        -Werror `
        -I (Join-Path $workspace 'Core\Inc') `
        (Join-Path $testDir 'test_terminal_autonomy.c') `
        (Join-Path $workspace 'Core\Src\terminal_autonomy.c') `
        (Join-Path $workspace 'Core\Src\service_control.c') `
        (Join-Path $workspace 'Core\Src\service_alarm.c') `
        (Join-Path $workspace 'Core\Src\protocol_lora.c') `
        -o $output
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & $output
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
