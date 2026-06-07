$testDir = "D:\CODE\zero-c\out\TEST"
$tests = @(
    "test_sqlite_db",
    "test_at_log_writer",
    "test_diag_snapshot_writer",
    "test_llm_chat_writer",
    "test_report_html",
    "test_config_io"
)
foreach ($t in $tests) {
    $exe = Join-Path $testDir "$t.exe"
    Write-Output "=========================================="
    Write-Output "[P6] $t"
    Write-Output "=========================================="
    $p = Start-Process -FilePath $exe -NoNewWindow -PassThru -Wait `
            -RedirectStandardOutput "$testDir\$t.out.txt" `
            -RedirectStandardError "$testDir\$t.err.txt"
    $code = $p.ExitCode
    $out = Get-Content "$testDir\$t.out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$testDir\$t.err.txt" -Raw -ErrorAction SilentlyContinue
    Write-Output "--- exit code: $code ---"
    if ($out) { Write-Output "--- stdout ---"; Write-Output $out }
    if ($err) { Write-Output "--- stderr ---"; Write-Output $err }
    $combined = ($out + "`n" + $err)
    if ($combined -match 'skip \(sqlite stub mode\)|skip \(SQLite stub mode\)') {
        Write-Output "RESULT: SKIP (sqlite stub mode - expected)"
    } elseif ($code -eq 0 -and $combined -notmatch 'FAIL|ERROR|fatal|assertion failed') {
        Write-Output "RESULT: PASS"
    } else {
        Write-Output "RESULT: FAIL (exit $code)"
    }
    Write-Output ""
}
