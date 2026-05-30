# run_agent_optimization.ps1
# Agent駆動の最適化反復で、各試行のベンチ実行・対象スコープ抽出・結果レポート生成を自動化する。
param(
    [string]$WorkflowConfigPath = "tools\\perf\\agent_optimization_workflow.json"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ScopeMetric {
    param(
        [Parameter(Mandatory = $true)][string]$JsonPath,
        [Parameter(Mandatory = $true)][string]$Scope
    )
    if (-not (Test-Path $JsonPath)) {
        throw "JSON not found: $JsonPath"
    }
    $data = Get-Content -Raw -Path $JsonPath | ConvertFrom-Json
    if (-not $data.markers) {
        throw "markers field not found in: $JsonPath"
    }
    $marker = $data.markers | Where-Object { $_.scope -eq $Scope } | Select-Object -First 1
    if (-not $marker) {
        throw "Target scope not found: $Scope in $JsonPath"
    }
    return [PSCustomObject]@{
        Scope  = $marker.scope
        AvgMs  = [double]$marker.avg_ms
        P95Ms  = [double]$marker.p95_ms
        MaxMs  = [double]$marker.max_ms
        FrameId = [int64]$marker.frame_id
    }
}

function New-ReportMarkdown {
    param(
        [Parameter(Mandatory = $true)]$Rows,
        [Parameter(Mandatory = $true)][string]$TargetScope,
        [Parameter(Mandatory = $true)][double]$BaselineAvgMs,
        [Parameter(Mandatory = $true)][double]$TargetRatio,
        [Parameter(Mandatory = $true)][string]$WorkflowName
    )

    $goalMs = $BaselineAvgMs * $TargetRatio
    $best = $Rows | Sort-Object AvgMs | Select-Object -First 1
    $achieved = ($best.RatioVsBaseline -le $TargetRatio)
    $achievedText = if ($achieved) { "達成" } else { "未達" }
    $bestReductionPct = (1.0 - [double]$best.RatioVsBaseline) * 100.0

    $lines = @()
    $lines += "# Agent最適化レポート: $WorkflowName"
    $lines += ""
    $lines += "## 目的"
    $lines += ("- 対象スコープ: ``{0}``" -f $TargetScope)
    $lines += ("- 目標: baseline比 {0:P2} 以下（avg_ms <= {1:F6}）" -f $TargetRatio, $goalMs)
    $lines += ("- baseline avg_ms: {0:F6}" -f $BaselineAvgMs)
    $lines += ""
    $lines += "## 結果"
    $lines += "- 判定: **$achievedText**"
    $lines += ("- 最良 avg_ms: {0:F6}" -f [double]$best.AvgMs)
    $lines += ("- baseline比: {0:P2}" -f [double]$best.RatioVsBaseline)
    $lines += ("- 改善率: -{0:F2}%" -f $bestReductionPct)
    $lines += ("- 最良試行: iteration {0} ({1})" -f [int]$best.Iteration, $best.Tag)
    $lines += ""
    $lines += "## 試行一覧"
    $lines += "| iteration | tag | change_summary | avg_ms | p95_ms | max_ms | baseline比 | artifact_dir |"
    $lines += "| --- | --- | --- | ---: | ---: | ---: | ---: | --- |"
    foreach ($r in ($Rows | Sort-Object Iteration)) {
        $lines += ("| {0} | {1} | {2} | {3:F6} | {4:F6} | {5:F6} | {6:P2} | ``{7}`` |" -f `
            [int]$r.Iteration, $r.Tag, $r.ChangeSummary, [double]$r.AvgMs, [double]$r.P95Ms, [double]$r.MaxMs, [double]$r.RatioVsBaseline, $r.ArtifactDir)
    }
    return ($lines -join "`r`n") + "`r`n"
}

$repoRoot = (Resolve-Path ".").Path
$workflowPath = Join-Path $repoRoot $WorkflowConfigPath
if (-not (Test-Path $workflowPath)) {
    throw "Workflow config not found: $workflowPath"
}

$workflow = Get-Content -Raw -Path $workflowPath | ConvertFrom-Json
if (-not $workflow.name) { throw "workflow.name is required." }
if (-not $workflow.target_scope) { throw "workflow.target_scope is required." }
if (-not $workflow.baseline_json) { throw "workflow.baseline_json is required." }
if (-not $workflow.goal_target_ratio) { throw "workflow.goal_target_ratio is required." }
if (-not $workflow.max_iterations) { throw "workflow.max_iterations is required." }
if (-not $workflow.attempts) { throw "workflow.attempts is required." }

$targetScope = [string]$workflow.target_scope
$baselineJson = Join-Path $repoRoot ([string]$workflow.baseline_json)
$goalTargetRatio = [double]$workflow.goal_target_ratio
$maxIterations = [int]$workflow.max_iterations
$appWindowStyle = if ($workflow.app_window_style) { [string]$workflow.app_window_style } else { "Hidden" }
$runBenchmarkScriptRel = if ($workflow.run_benchmark_script) { [string]$workflow.run_benchmark_script } else { "tools\\perf\\run_benchmark.ps1" }
$runBenchmarkScript = Join-Path $repoRoot $runBenchmarkScriptRel
$benchmarkOutputRootRel = if ($workflow.benchmark_output_root) { [string]$workflow.benchmark_output_root } else { "artifacts\\perf" }
$benchmarkOutputRoot = Join-Path $repoRoot $benchmarkOutputRootRel
$outputRootRel = if ($workflow.output_root) { [string]$workflow.output_root } else { "artifacts\\perf\\agent_opt" }
$outputRoot = Join-Path $repoRoot $outputRootRel
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (-not (Test-Path $benchmarkOutputRoot)) {
    New-Item -ItemType Directory -Force -Path $benchmarkOutputRoot | Out-Null
}

if (-not (Test-Path $runBenchmarkScript)) {
    throw "run_benchmark script not found: $runBenchmarkScript"
}

$baselineMetric = Get-ScopeMetric -JsonPath $baselineJson -Scope $targetScope
$baselineAvgMs = [double]$baselineMetric.AvgMs
if ($baselineAvgMs -le 0.0) {
    throw "Baseline avg_ms must be > 0. scope=$targetScope"
}

$sessionStamp = Get-Date -Format "yyyyMMdd_HHmmss"
$sessionDir = Join-Path $outputRoot ("session_" + $sessionStamp)
New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null

$results = @()
$results += [PSCustomObject]@{
    Iteration = 0
    Tag = "baseline"
    ChangeSummary = "baseline"
    AvgMs = $baselineMetric.AvgMs
    P95Ms = $baselineMetric.P95Ms
    MaxMs = $baselineMetric.MaxMs
    RatioVsBaseline = 1.0
    ArtifactDir = (Split-Path -Parent $baselineJson)
}

$attemptRows = @($workflow.attempts | Where-Object { $_.enabled -ne $false } | Sort-Object iteration)
if ($attemptRows.Count -eq 0) {
    throw "No enabled attempts found in workflow.attempts."
}

$executed = 0
foreach ($attempt in $attemptRows) {
    if ($executed -ge $maxIterations) { break }
    if (-not $attempt.tag) { throw "attempt.tag is required." }
    if (-not $attempt.change_summary) { throw "attempt.change_summary is required." }

    $before = @{}
    Get-ChildItem -Directory -Path $benchmarkOutputRoot | ForEach-Object { $before[$_.FullName] = $true }

    Write-Host ("[opt] Iteration {0}: {1}" -f [int]$attempt.iteration, [string]$attempt.tag)
    & powershell -ExecutionPolicy Bypass -File $runBenchmarkScript `
        -Tag ([string]$attempt.tag) `
        -BaselineJson $baselineJson `
        -OutputRoot $benchmarkOutputRootRel `
        -AppWindowStyle $appWindowStyle
    if ($LASTEXITCODE -ne 0) {
        throw "run_benchmark failed at attempt tag=$($attempt.tag)"
    }

    $newArtifacts = Get-ChildItem -Directory -Path $benchmarkOutputRoot | Where-Object { -not $before.ContainsKey($_.FullName) } | Sort-Object LastWriteTime -Descending
    $artifactDir = if ($newArtifacts.Count -gt 0) { $newArtifacts[0].FullName } else { (Get-ChildItem -Directory -Path $benchmarkOutputRoot | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName }
    if (-not $artifactDir) { throw "Artifact directory could not be resolved." }

    $candidateJson = Join-Path $artifactDir "candidate.json"
    $metric = Get-ScopeMetric -JsonPath $candidateJson -Scope $targetScope
    $ratio = [double]$metric.AvgMs / $baselineAvgMs

    $results += [PSCustomObject]@{
        Iteration = [int]$attempt.iteration
        Tag = [string]$attempt.tag
        ChangeSummary = [string]$attempt.change_summary
        AvgMs = [double]$metric.AvgMs
        P95Ms = [double]$metric.P95Ms
        MaxMs = [double]$metric.MaxMs
        RatioVsBaseline = $ratio
        ArtifactDir = $artifactDir
    }
    $executed++

    if ($ratio -le $goalTargetRatio) {
        Write-Host "[opt] Goal reached. Stop iterations."
        break
    }
}

$reportJson = Join-Path $sessionDir "optimization_results.json"
$results | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path $reportJson

$reportMd = Join-Path $sessionDir "optimization_report.md"
$md = New-ReportMarkdown -Rows $results -TargetScope $targetScope -BaselineAvgMs $baselineAvgMs -TargetRatio $goalTargetRatio -WorkflowName ([string]$workflow.name)
Set-Content -Path $reportMd -Encoding UTF8 -Value $md

Write-Host "[opt] Done."
Write-Host "  Session: $sessionDir"
Write-Host "  JSON: $reportJson"
Write-Host "  Report: $reportMd"
