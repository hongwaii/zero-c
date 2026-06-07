$testDir = "D:\CODE\zero-c\out\TEST"
# All known test executables
$tests = @(
    @{Name="test_strbuf"; Phase="P0"},
    @{Name="test_json_roundtrip"; Phase="P0"},
    @{Name="test_i18n"; Phase="P0"},
    @{Name="test_ringbuf"; Phase="P0"},
    @{Name="test_ncm_enumerate"; Phase="P1"},
    @{Name="test_at_parser"; Phase="P1"},
    @{Name="test_at_session"; Phase="P1"},
    @{Name="test_diag_state"; Phase="P3"},
    @{Name="test_diag_state_ext"; Phase="P3"},
    @{Name="test_diag_ping"; Phase="P3"},
    @{Name="test_diag_ssl"; Phase="P3"},
    @{Name="test_diag_sms"; Phase="P3"},
    @{Name="test_diag_log"; Phase="P3"},
    @{Name="test_diag_health"; Phase="P3"},
    @{Name="test_llm_dpapi"; Phase="P5"},
    @{Name="test_llm_sse"; Phase="P5"},
    @{Name="test_llm_tool"; Phase="P5"},
    @{Name="test_llm_client_mock"; Phase="P5"},
    @{Name="test_provider_config"; Phase="P5"},
    @{Name="test_sqlite_db"; Phase="P6"},
    @{Name="test_at_log_writer"; Phase="P6"},
    @{Name="test_diag_snapshot_writer"; Phase="P6"},
    @{Name="test_llm_chat_writer"; Phase="P6"},
    @{Name="test_report_html"; Phase="P6"},
    @{Name="test_config_io"; Phase="P6"}
)
$summary = @()
foreach ($t in $tests) {
    $exe = Join-Path $testDir "$($t.Name).exe"
    if (-not (Test-Path $exe)) {
        $summary += [PSCustomObject]@{Phase=$t.Phase; Name=$t.Name; Status="MISSING"}
        continue
    }
    $p = Start-Process -FilePath $exe -NoNewWindow -PassThru -Wait `
            -RedirectStandardOutput "$testDir\$($t.Name).out.txt" `
            -RedirectStandardError "$testDir\$($t.Name).err.txt"
    $code = $p.ExitCode
    $out = Get-Content "$testDir\$($t.Name).out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$testDir\$($t.Name).err.txt" -Raw -ErrorAction SilentlyContinue
    $combined = ($out + "`n" + $err)
    $status = "PASS"
    if ($combined -match 'skip \(sqlite stub mode') { $status = "SKIP-stub" }
    elseif ($code -ne 0) { $status = "FAIL($code)" }
    elseif ($combined -match 'FAIL|assertion failed|FATAL|error:') { $status = "FAIL" }
    $summary += [PSCustomObject]@{Phase=$t.Phase; Name=$t.Name; Status=$status; ExitCode=$code}
}
$summary | Group-Object Phase | ForEach-Object {
    Write-Output "=== $($_.Name) ==="
    $_.Group | Format-Table -AutoSize
}
$failCount = ($summary | Where-Object { $_.Status -like "FAIL*" }).Count
$passCount = ($summary | Where-Object { $_.Status -eq "PASS" }).Count
$skipCount = ($summary | Where-Object { $_.Status -eq "SKIP-stub" }).Count
$missCount = ($summary | Where-Object { $_.Status -eq "MISSING" }).Count
Write-Output ""
Write-Output "=== TOTAL ==="
Write-Output "PASS: $passCount"
Write-Output "SKIP (sqlite stub): $skipCount"
Write-Output "FAIL: $failCount"
Write-Output "MISSING: $missCount"
