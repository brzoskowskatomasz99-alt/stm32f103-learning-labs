$ErrorActionPreference = 'Stop'
$ws = 'E:\TEMPLATE\Template'
$inc = Join-Path $ws 'Core\Inc'
$inc2 = Join-Path $ws 'MDK-ARM\UserCode'
$out = Join-Path $env:TEMP 'asan_suite.exe'
$fail = 0

$suites = @(
    @{ n='protocol_lora';   s=@('test_protocol_lora.c','protocol_lora.c') },
    @{ n='bridge_mqtt';     s=@('test_bridge_mqtt.c','bridge_mqtt.c','protocol_lora.c') },
    @{ n='mqtt_subscribe';  s=@('test_mqtt_subscribe.c') },
    @{ n='service_control'; s=@('test_service_control.c','service_control.c') },
    @{ n='service_alarm';   s=@('test_service_alarm.c','service_alarm.c') },
    @{ n='command_link';    s=@('test_command_link.c','command_link.c','protocol_lora.c') },
    @{ n='terminal_table';  s=@('test_terminal_table.c','terminal_table.c') },
    @{ n='link_stats';      s=@('test_link_stats.c','link_stats.c') },
    @{ n='alarm_registry';  s=@('test_alarm_registry.c','alarm_registry.c','protocol_lora.c') },
    @{ n='ui_oled';         s=@('test_ui_oled.c','ui_oled.c','alarm_registry.c','bridge_mqtt.c','protocol_lora.c') },
    @{ n='gateway_data';    s=@('test_gateway_data.c','gateway_data.c','protocol_lora.c') },
    @{ n='terminal_autonomy'; s=@('test_terminal_autonomy.c','terminal_autonomy.c','service_control.c','service_alarm.c','protocol_lora.c') }
)

foreach ($t in $suites) {
    $dir = Join-Path $ws ('tests\' + $t.n)
    $srcs = @()
    foreach ($f in $t.s) {
        $cand = Join-Path $dir $f
        if (-not (Test-Path $cand)) { $cand = Join-Path $ws ('Core\Src\' + $f) }
        $srcs += $cand
    }
    & clang -std=c11 -Wall -Wextra -Werror '-fsanitize=address,undefined' -fno-omit-frame-pointer `
        -I $inc -I $inc2 $srcs -o $out 2>$null
    if ($LASTEXITCODE -ne 0) { Write-Output "$($t.n) : COMPILE_FAIL"; $fail++; continue }
    $r = & $out 2>&1 | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { Write-Output "$($t.n) : RUNTIME_FAIL ($r)"; $fail++ }
    else { Write-Output "$($t.n) : PASS (asan/ubsan)" }
    Remove-Item $out -Force -ErrorAction SilentlyContinue
}
Write-Output "FAILED=$fail"
