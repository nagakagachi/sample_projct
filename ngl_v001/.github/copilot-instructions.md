# InstantRdv project instructions

このリポジトリで InstantRdv 周辺を扱う際は、以下の構造と用語を前提に作業すること。

## 対象領域

- InstantRdv の主要シェーダは `ngl/shader/instant_rdv/` 配下にある。
- BBV デバッグ表示は `ngl/shader/instant_rdv/debug_util/voxel_debug_visualize_cs.hlsl`。
- InstantRdv の共通トレース / アドレス計算 / 補助関数は `ngl/shader/instant_rdv/instant_rdv_util.hlsli`。
- ランタイム側のデバッグ UI と dispatch パラメータ設定は `ngl/src/render/app/instant_rdv/instant_rdv.cpp`。

## 用語

- **InstantRdv**: Instant Raster Derived Voxel Scene 系の機能群全体。
- **BBV**: Bitmask Brick Voxel。bitmask と coarse occupancy を持つ voxel 構造。
- **Brick**: BBV の coarse セル。1 Brick は `k_bbv_per_voxel_resolution` 単位の fine voxel を内包する。
- **Fine voxel / bitmask voxel**: Brick 内の最細セル。
- **HiBrick**: 2x2x2 Brick cluster を 1 単位にした上位アクセラレータ。
- **WCP**: World/Window/whatever current project convention の probe 系 voxel 構造。BBV とは別カテゴリ。
- **SSP / Screen Space Probe**: 画面空間側の probe 更新 / デバッグ機能。

## BBV バッファ構造

- BBV 本体バッファの並びは `[bitmask region][brick data region][hibrick data region]`。
- Brick / HiBrick の count は bitmask 更新後に後段パスで再構築する前提。
- HiBrick data region は logical 2x2x2 Brick cluster 順で保持する。

## BBV trace の整理方針

- 通常の BBV トレース入口:
  - `trace_bbv`
  - `trace_bbv_initial_hit_avoidance`
  - `trace_bbv_inverse_bit`
- HiBrick を使う入口:
  - `trace_bbv_hibrick`
  - `trace_bbv_initial_hit_avoidance_hibrick`
  - `trace_bbv_dev_hibrick`
- 開発用の比較入口:
  - `trace_bbv_dev`
  - `trace_bbv_hibrick_no_skip`
  - `trace_bbv_dev_hibrick_no_skip`

trace 系を触る場合は、通常版 / 初期ヒット回避版 / dev 版で意味が揃っているか確認すること。

## 最近の重要事項

- BBV デバッグ表示は HiBrick 版トレースへ寄せて整理済み。
- BBV debug sub mode 1 は **非HiBrick版での最細セル色分け表示**。
- 以前あった HiBrick 重複モードや skip 無効比較モードは整理済みのため、番号追加時は `instant_rdv.cpp` の sub mode 上限と合わせて更新すること。

## シェーダファイルのエンコード運用（重要）

- `.hlsl` / `.hlsli` は **UTF-8（BOMなし）固定**で扱うこと。
- 置換スクリプトや自動書き戻しで BOM 付き UTF-8 に変化させないこと。
- `StreamReader.CurrentEncoding` をそのまま `StreamWriter` に渡す実装は No-BOM→BOM 化を起こしうるため避け、`UTF8Encoding(false)` を明示すること。
- シェーダ一括変更後は BOM 有無を検査してからビルドすること。
- このプロジェクトでは BOM 付与が DXC 経路で不整合を誘発し、`c0000374` の再現要因になった実績がある。

## DDA / reciprocal に関する注意

- `calc_safe_trace_ray_dir_inv` は名前に反して、現状は **旧来どおり `1.0 / ray_dir` を返す** 実装を採用している。
- ray dir の 0 軸を巨大値へ置き換える safe reciprocal を試したところ、HiBrick 有無に関係なく BBV デバッグ表示で特定カメラ角度に **1px の横線欠け** が発生した。
- DDA 側の開始セル、侵入位置、外側ステップ更新を切り分けても症状は消えず、reciprocal の扱いを戻すと解消した。
- BBV trace の境界欠けを見た場合は、DDA 本体より先に reciprocal の扱いを疑うこと。

## 編集時の期待

- InstantRdv 変更時は shader 側と `instant_rdv.cpp` 側の debug mode / 定数 / 呼び出し先の整合を保つこと。
- BBV debug 表示の mode を追加・削除したら、少なくとも `voxel_debug_visualize_cs.hlsl` と `instant_rdv.cpp` の両方を更新すること。
- trace 関数を整理するときは、コメントで「どのレイヤを走査するか」「debug 出力の各成分の意味」を簡潔に残すこと。
- **新規作成・編集するソースファイルは、ファイル先頭に「ファイル名」と「ファイル説明」のコメントを必ず記述すること。**
- **処理スコープ（主要ブロック）ごとに、「何をしているか」を簡素なコメントで記述すること。**

## Project TODO / backlog

- **BrickLocalAABB の追加**
  - 各 BBV Brick について occupied fine voxel の local min/max を保持し、Brick 内トレース開始位置や空領域スキップを改善する。
  - 8x8x8 / 512bit bitmask の leaf 走査を軽くするための次の高速化候補として扱う。

- **HiBrick trace の本番系比較と置き換え**
  - デバッグ表示以外の InstantRdv トレースでは、まだ `trace_bbv` 呼び出しが残っている箇所がある。
  - 少なくとも `bbv_removal_list_build_cs.hlsl`、`ss_probe_direct_sh_update_cs.hlsl`、`ss_probe_update_cs.hlsl`、`wcp_element_update_cs.hlsl`、`wcp_visible_surface_element_update_cs.hlsl` の性能比較対象にする。
  - `trace_bbv_hibrick` 適用後に性能とヒット整合性を確認し、問題なければ本番経路を段階的に置き換える。

- **HiBrick / Brick 充填率を使った VoxelCone trace の実装と検証**
  - HiBrick と Brick の occupancy ratio を使い、cone 幅に応じた近似トレース / 積分に使えるか検証する。
  - 品質と性能の両面で InstantRdv への適用可能性を確認する。

## Agent運用: ベンチマーク自動実行フロー（ローカル前提）

- Agent は、ユーザーの以下の表記ゆれを同義として扱うこと:
  - `benchmark`
  - `ベンチマーク`
  - `bench`
  - `perf-run`
  - `計測分析レポート`
- 上記が含まれる依頼を受けたら、**「benchmark機能を使った計測分析レポート」フローの要求**として解釈する。

### 正規化する内部タスク名

- 内部の実行名・ログ名は `benchmark` に統一する（外部入力はファジー受理）。

### 標準フロー（実装後の運用）

1. Releaseビルド
2. `sample_app --benchmark ...` 実行（**working directory は `sample_app` 固定**）
3. **最低 60 フレーム経過後**、`delta_time` が安定（既定: 30フレーム連続）または開始タイムアウト到達で計測開始
4. 自動終了
5. baseline/candidate比較レポートを保存

### 現在の主要CLI引数（benchmark）

- `--benchmark-warmup <frames>`
- `--benchmark-ready-delta-frames <frames>`
- `--benchmark-start-timeout-sec <seconds>`
- `--benchmark-measure <frames>`
- `--benchmark-output <dir>`
- `--benchmark-tag <label>`

### 成果物の標準出力先

- `artifacts/perf/<timestamp>/`
  - `baseline.json`
  - `candidate.json`
  - `report.md`
