# run_benchmark.ps1: Build sample_app, run benchmark mode, and generate analysis report.
param(
    [string]$SolutionPath = "ngl_v001.sln",
    [string]$Configuration = "Release",
    [string]$AppPath = "x64\\Release\\sample_app.exe",
    [string]$OutputRoot = "artifacts\\perf",
    [int]$WarmupFrames = 60,
    [int]$MeasureFrames = 600,
    [double]$StartTimeoutSeconds = 120,
    [int]$ReadyDeltaStableFrames = 30,
    [int]$TimeoutSec = 900,
    [string]$BaselineJson = "",
    [string]$Tag = "candidate"
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
    "--benchmark-start-timeout-sec", "$StartTimeoutSeconds",
    "--benchmark-measure", "$MeasureFrames",
    "--benchmark-ready-delta-frames", "$ReadyDeltaStableFrames",
    "--benchmark-output", "$candidateDir",
    "--benchmark-tag", "$Tag"
)

Write-Host "[perf] Running benchmark: $appExeAbs $($benchmarkArgs -join ' ')"
$appWorkingDir = Join-Path $repoRoot "sample_app"
$proc = Start-Process -FilePath $appExeAbs -ArgumentList $benchmarkArgs -PassThru -WorkingDirectory $appWorkingDir
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while (-not $proc.HasExited -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
}
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id
    throw "Benchmark timed out after $TimeoutSec sec."
}

$candidateJson = Join-Path $candidateDir "benchmark_gpu_markers.json"
if (-not (Test-Path $candidateJson)) {
    throw "Benchmark output JSON not found (exit code=$($proc.ExitCode)): $candidateJson"
}
if ($proc.ExitCode -ne 0) {
    Write-Warning "Benchmark process exited with code $($proc.ExitCode), but output JSON exists. Continue."
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
