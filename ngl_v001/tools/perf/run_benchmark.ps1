# run_benchmark.ps1
# 1) sample_app を Release ビルド
# 2) benchmark モード実行と生マーカー出力の回収
# 3) 比較用に整形して report.md を生成
#
# 出力フォルダ構成:
#   artifacts\perf\<timestamp>\
#     candidate\benchmark_gpu_markers.json   # sample_app が出力する生 JSON
#     candidate\benchmark_gpu_markers.csv    # sample_app が出力する生 CSV
#     candidate.json                          # compare スクリプト用の候補 JSON
#     baseline.json                           # ベースライン比較時のみ作成
#     report.md                               # compare_gpu_markers.py の要約レポート
#     sample_app.stdout.log                   # sample_app の標準出力ログ
#     sample_app.stderr.log                   # sample_app の標準エラーログ
param(
    [string]$SolutionPath = "ngl_v001.sln",
    [string]$Configuration = "Release",
    [string]$AppPath = "x64\\Release\\sample_app.exe",
    [string]$OutputRoot = "artifacts\\perf",
    [int]$WarmupFrames = 60,
    [int]$MeasureFrames = 600,
    [double]$MinReadySeconds = 5.0,
    [double]$StartTimeoutSeconds = 120,
    [int]$ReadyDeltaStableFrames = 30,
    [int]$TimeoutSec = 900,
    [string]$BaselineJson = "",
    [string]$Tag = "candidate",
    [ValidateSet("Hidden", "Minimized", "Normal")]
    [string]$AppWindowStyle = "Hidden"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-PythonCommand {
    $py = Get-Command python -ErrorAction SilentlyContinue
    if ($py) { return @{ Exe = "python"; Args = @() } }
    $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
    if ($pyLauncher) { return @{ Exe = "py"; Args = @("-3") } }
    throw "Python command was not found."
}

$repoRoot = (Resolve-Path ".").Path
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$artifactDir = Join-Path $repoRoot (Join-Path $OutputRoot $timestamp)
$candidateDir = Join-Path $artifactDir "candidate"
# 生出力は candidate\ に集約し、比較/要約用ファイルは artifact 直下に配置する。
New-Item -ItemType Directory -Force -Path $candidateDir | Out-Null

Write-Host "[perf] Building $Configuration..."
& msbuild $SolutionPath /t:Build /p:Configuration=$Configuration /m /nologo | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

$appExeAbs = Join-Path $repoRoot $AppPath
if (-not (Test-Path $appExeAbs)) {
    throw "sample_app executable not found: $appExeAbs"
}

$benchmarkArgs = @(
    "--benchmark",
    "--benchmark-warmup", "$WarmupFrames",
    "--benchmark-min-ready-sec", "$MinReadySeconds",
    "--benchmark-start-timeout-sec", "$StartTimeoutSeconds",
    "--benchmark-measure", "$MeasureFrames",
    "--benchmark-ready-delta-frames", "$ReadyDeltaStableFrames",
    "--benchmark-output", "$candidateDir",
    "--benchmark-tag", "$Tag"
)

Write-Host "[perf] Running benchmark: $appExeAbs $($benchmarkArgs -join ' ')"
$appWorkingDir = Join-Path $repoRoot "sample_app"
# コンソールを Hidden/Minimized で起動してもログを必ず保存する。
# シェーダ読み込み・コンパイル失敗は stderr 側を確認する。
$appStdoutLog = Join-Path $artifactDir "sample_app.stdout.log"
$appStderrLog = Join-Path $artifactDir "sample_app.stderr.log"
$proc = Start-Process -FilePath $appExeAbs -ArgumentList $benchmarkArgs -PassThru -WorkingDirectory $appWorkingDir -WindowStyle $AppWindowStyle -RedirectStandardOutput $appStdoutLog -RedirectStandardError $appStderrLog
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while (-not $proc.HasExited -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
}
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id
    throw "Benchmark timed out after $TimeoutSec sec."
}
$proc.Refresh()
$exitCode = $proc.ExitCode
$exitCodeText = if ($null -eq $exitCode) { "unknown" } else { "$exitCode" }

$candidateJson = Join-Path $candidateDir "benchmark_gpu_markers.json"
if (-not (Test-Path $candidateJson)) {
    throw "Benchmark output JSON not found (exit code=$exitCodeText): $candidateJson"
}
if ($null -eq $exitCode -or $exitCode -ne 0) {
    # 現状は、出力生成後に非0終了するケースがある。
    # JSON が存在する場合は成果物を優先して処理継続する。
    Write-Warning "Benchmark process exited with code $exitCodeText, but output JSON exists. Continue."
}
$candidateFlatJson = Join-Path $artifactDir "candidate.json"
Copy-Item -Force $candidateJson $candidateFlatJson

$baselineFlatJson = ""
if ($BaselineJson -and (Test-Path $BaselineJson)) {
    $baselineFlatJson = Join-Path $artifactDir "baseline.json"
    Copy-Item -Force $BaselineJson $baselineFlatJson
}

$reportPath = Join-Path $artifactDir "report.md"
$compareScript = Join-Path $repoRoot "tools\\perf\\compare_gpu_markers.py"
if (-not (Test-Path $compareScript)) {
    throw "Compare script not found: $compareScript"
}

$pyCmd = Resolve-PythonCommand
$compareArgs = @($compareScript, "--candidate", $candidateFlatJson, "--output", $reportPath)
if ($baselineFlatJson) {
    $compareArgs += @("--baseline", $baselineFlatJson)
}
Write-Host "[perf] Generating report..."
& $pyCmd.Exe @($pyCmd.Args + $compareArgs)
if ($LASTEXITCODE -ne 0) {
    throw "compare_gpu_markers.py failed."
}

Write-Host "[perf] Done."
Write-Host "  Artifact: $artifactDir"
Write-Host "  Candidate: $candidateFlatJson"
if ($baselineFlatJson) { Write-Host "  Baseline: $baselineFlatJson" }
Write-Host "  Report: $reportPath"
Write-Host "  App stdout: $appStdoutLog"
Write-Host "  App stderr: $appStderrLog"
