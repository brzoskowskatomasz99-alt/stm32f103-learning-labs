$ErrorActionPreference = 'Stop'

$testDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = (Resolve-Path (Join-Path $testDir '..\..')).Path
$output = Join-Path $testDir 'mqtt_subscribe_tests.exe'

try {
    & clang -std=c11 -I (Join-Path $workspace 'Core\Inc') `
        -I (Join-Path $workspace 'MDK-ARM\UserCode') `
        (Join-Path $testDir 'test_mqtt_subscribe.c') -o $output
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $output
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
