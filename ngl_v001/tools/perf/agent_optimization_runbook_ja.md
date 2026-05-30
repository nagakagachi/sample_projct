# Agent駆動GPU最適化 Runbook（日本語）

## 目的
「実装 → ベンチ計測 → 対象スコープ抽出 → 比較記録」を同じ型で繰り返し、再現性のある最適化ループを回す。

> この運用は特定シェーダ専用ではなく、任意のGPUスコープ（Graphics/Compute問わず）に適用できる。

## 構成（3層）
1. 運用Runbook（このファイル）  
2. 実行テンプレート（`agent_optimization_workflow.example.json`）  
3. 自動実行スクリプト（`run_agent_optimization.ps1`）

## 初回セットアップ
1. `agent_optimization_workflow.example.json` をコピーして `tools\perf\agent_optimization_workflow.json` を作成
2. 以下を編集
   - `target_scope`: 最適化対象GPUスコープ
   - `baseline_json`: baselineの `candidate.json` パス
   - `benchmark_output_root`: `run_benchmark.ps1` の成果物出力先ルート
   - `goal_target_ratio`: 目標（例: 0.5 = 半減）
   - `max_iterations`: 最大試行数
   - `attempts`: 試行の `iteration/tag/change_summary`

## 実行手順
1. 各試行のコード変更を反映する
2. `tools\perf\agent_optimization_workflow.json` の `attempts` を更新
3. 実行:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\tools\perf\run_agent_optimization.ps1 -WorkflowConfigPath "tools\perf\agent_optimization_workflow.json"
   ```
4. 出力確認:
   - `artifacts\perf\agent_opt\session_<timestamp>\optimization_results.json`
   - `artifacts\perf\agent_opt\session_<timestamp>\optimization_report.md`

## 失敗時の確認順
1. `artifacts\perf\<timestamp>\sample_app.stderr.log`（シェーダコンパイル/実行エラー）
2. `candidate\benchmark_gpu_markers.json` の生成有無
3. `target_scope` の表記揺れ（完全一致）

## 運用上のルール
- 1試行1変更を基本にし、`change_summary` は具体的に書く
- 悪化試行も消さずに残す（学習コスト削減）
- 目標達成時点で停止してよい

## 将来の調整について
調整は前提で問題なし。推奨順は次の通り。
1. まず `agent_optimization_workflow.json` の値を調整（閾値、試行数、対象スコープ）
2. 次に `run_agent_optimization.ps1` の集計項目を拡張（例: p50、frame_id、追加判定）
3. 必要なら `run_benchmark.ps1` / `compare_gpu_markers.py` の出力仕様を拡張

この順にすると、既存の計測資産を壊さずに段階的に改善できる。
