# FSP Visible Surface Optimization Plan

## 背景

InstantRDV の Frustum Space Probe(FSP) は、depth buffer から可視サーフェイスと交差する可能性が高い FSP cell を抽出し、その cell に probe を割り当てる。現行の `fsp_screen_space_pass_cs.hlsl` と試作3-pass版は動作上は問題ないが、負荷の大半が screen-space accumulate shader に集中している。

計測結果では、新設3-pass版の総負荷削減は数%程度で、3-pass内の95%以上が `fsp_screen_space_accumulate_cs.hlsl` に集中した。したがって、単に atomic/list 更新を別passへ分散するだけでは不十分であり、全pixelで world復元、cascade cell算出、depth hint計算、atomic を行う構造自体を見直す。

## 品質方針

- 可視サーフェイスのカバー率低下は品質低下に直結するため、安易なpixel skipや代表pixelのみの処理は避ける。
- 最適化は「全pixelのdepth情報を読む/保守的に集約する」ことを前提にする。
- 取り逃しよりも過剰markを許容する。余分なprobeが増える方向は品質安全側として扱う。
- 既存の legacy 1-pass (`fsp_screen_space_pass_cs.hlsl`) と比較できる切替は維持する。

## 現行負荷分析

`fsp_screen_space_accumulate_cs.hlsl` の主な負荷は以下。

- 全pixelで `TexHardwareDepth.Load`
- 全有効pixelで view-z 復元、view ray算出、world position復元
- 全pixel x cascade count で `FspTryGetGlobalCellIndexFromWorldPos`
- cascadeごとの `FspBuildDepthHintPackedKey`
  - `FspCalcCellCenterWs`
  - toroidal mapping
  - normalize/rsqrt/dot/quantize
- cascadeごとの groupshared hash 初期化
- cascadeごとの複数 `GroupMemoryBarrierWithGroupSync`
- global buffer への visible frame / depth hint atomic

重要な観察として、現状の `fsp_pre_update_cs.hlsl` では `depth_hint_packed_key` は主に「visible surface anchor があるか」の判定に使われており、pixel id や詳細metricは probe位置決定に強く使われていない。したがって、depth hint metric計算は現行品質に対して過剰な可能性が高い。

## 第一案: Full-res Depth Tile Slice Mask

Status: initial implementation added.

### 狙い

全pixelのdepthを読むことは維持しつつ、重い処理を pixel 数スケールから tile/slice 数スケールへ移す。

Depthをtile単位で depth slice bitmask に保守的集約し、後段で tile frustum x depth slice から FSP cell をmarkする。pixelを捨てず、tile内に存在したdepth sliceは全て保持するため、可視サーフェイスの取り逃しを避けやすい。

### 推奨pass構成

#### Pass A: `fsp_depth_tile_slice_mask_cs.hlsl`

役割:

- full-res depthを全pixel読む。
- 8x8 または 16x16 tileごとに depth slice mask を作る。
- 出力は tileごとの小さいbuffer。

候補出力:

```hlsl
// typed buffer案
// TileSliceMask[0] = tile count or unused
// TileSliceMask[1 + tile_index * N + i] = slice mask word
RWBuffer<uint> RWFspDepthTileSliceMask;
```

初期案:

- tile size: 8x8
- slice count: 32
- slice mapping: view_z の linear ではなく log寄りを優先
- 1 tile = `uint slice_mask`
- invalid/sky depth は無視

注意:

- tile内全pixelを読む。
- pixel単位のworld復元はしない。
- cascade処理はしない。
- global atomicはtile内bit OR程度に抑える。可能ならgroupsharedでtile内集約後、tile leaderが1回書く。

#### Pass B: `fsp_tile_slice_cell_mark_cs.hlsl`

役割:

- tileごとの slice mask を読み、set bit ごとに保守的な view-space depth interval を作る。
- tile screen範囲 + depth interval から conservative world-space frustum/AABB を作る。
- 各cascadeのgridへ投影し、交差しうるcell範囲をmarkする。

候補出力:

```hlsl
RWBuffer<uint> RWFspCellVisibleFrameBuffer;
RWBuffer<uint> RWFspCellDepthHintBuffer;
```

初期実装方針:

- depth slice interval の near/far 4隅を world space 復元し、world AABB を作る。
- その AABB を FSP grid cell range へ変換する。
- range内cellを `RWFspCellVisibleFrameBuffer[cell] = frame_count` でmark。
- `RWFspCellDepthHintBuffer[cell] = 0` など sentinel以外へ設定。
- 過剰markを許容し、取り逃し回避を優先する。

リスク:

- tile x slice x cascade x cell range が増えすぎる可能性。
- tile size/slice count/cell range clamp の調整が必要。
- thin geometry のcoverageは維持しやすいが、depth discontinuityのあるtileでは過剰markが増える。

#### Pass C: `fsp_screen_space_finalize_cs.hlsl`

役割:

- 既存finalizeを流用。
- `FspCellVisibleFrameBuffer[cell] == frame_count` のcellを visible list 化。
- `RWFspCellStateBuffer` へ互換mirror。

### 実装ステップ

1. tile slice mask buffer を C++側に追加する。
2. tile slice mask pass のPSO/dispatchを追加する。
3. tile slice cell mark pass のPSO/dispatchを追加する。
4. Debug UIに mode を追加する。
   - Legacy 1-pass
   - Current pixel accumulate 3-pass
   - Tile slice mask
5. `fsp_screen_space_finalize_cs.hlsl` は共通finalizeとして維持する。
6. benchmarkで以下を比較する。
   - legacy 1-pass
   - current 3-pass
   - tile slice mask

### 実装メモ

- Debug UI の `Visible Surface Mode` で以下を切り替える。
  - `0`: Legacy 1-pass
  - `1`: Pixel accumulate multipass
  - `2`: Full-res depth tile slice mask
- 追加shader:
  - `ngl_v001/ngl/shader/instant_rdv/fsp/fsp_depth_tile_slice_mask_cs.hlsl`
  - `ngl_v001/ngl/shader/instant_rdv/fsp/fsp_tile_slice_cell_mark_cs.hlsl`
- 追加buffer:
  - `fsp_depth_tile_slice_mask_buffer_`
- 初期実装は 8x8 tile / 32 log depth slices。

### Model A に任せる作業プロンプト

```
あなたは sample_project_agent リポジトリで InstantRDV の FSP visible surface 抽出を高速化する担当です。

最初の実装対象は Full-res Depth Tile Slice Mask 方式です。
可視サーフェイスのカバー率低下を避けるため、pixel skipや代表pixelのみの利用は禁止です。tile内の全pixel depthを読み、depth slice bitmaskへ保守的に集約してください。

参照する計画書:
- fsp_visible_surface_optimization_plan.md

主な対象ファイル:
- ngl_v001/ngl/shader/instant_rdv/fsp/fsp_screen_space_accumulate_cs.hlsl
- ngl_v001/ngl/shader/instant_rdv/fsp/fsp_screen_space_finalize_cs.hlsl
- ngl_v001/ngl/src/render/app/instant_rdv/instant_rdv.cpp
- ngl_v001/ngl/include/render/app/instant_rdv/instant_rdv.h
- ngl_v001/ngl/shader/instant_rdv/instant_rdv_util.hlsli

実装要件:
1. 新規pass `fsp_depth_tile_slice_mask_cs.hlsl` を追加する。
2. 新規pass `fsp_tile_slice_cell_mark_cs.hlsl` を追加する。
3. tile内全pixel depthを読み、32bit depth slice maskへ集約する。
4. tile slice maskからFSP cell visible frame/depth hint work bufferへ保守的にmarkする。
5. 既存finalize passを再利用し、visible list化する。
6. Debug UIで legacy/current multipass/tile-slice を切り替え可能にする。
7. 既存legacy 1-pass経路は比較用に残す。
8. .hlsl/.hlsliはUTF-8 BOMなしを維持する。
9. 変更箇所には処理意図が分かる簡潔なコメントを追加する。

品質条件:
- サーフェイスcoverage低下を避ける。
- 取り逃しより過剰markを許容する。
- CPU側バインド名とshader側宣言の整合を確認する。
- UAV barrier / SRV/UAV利用順を確認する。

計測観点:
- FspVisibleSurfaceProcessing total
- tile slice mask pass
- tile slice cell mark pass
- finalize pass
- visible surface cell count
- active probe count
```

## 第三案: Cell-driven + Depth Pyramid Min/Max

### 狙い

screen pixel駆動ではなく、FSP cell駆動で可視サーフェイス交差を判定する。Depth pyramid min/max を使い、cellのscreen投影範囲にsurface depthが存在するかを保守的に判定する。

### 推奨pass構成

#### Pass A: Depth Min/Max Pyramid Build

役割:

- full-res depthから min/max pyramid を構築する。
- Reverse-Z前提を明示し、view-zまたはdevice-zのどちらでpyramidを持つか統一する。

候補:

- R32G32_FLOAT texture
  - x = min view_z
  - y = max view_z
- もしくは R32_UINT packed depth range

注意:

- Reverse-Zと sky depth の扱いを明確化する。
- invalid depth をrangeに混ぜない。

#### Pass B: FSP Cell Projection Test

役割:

- FSP cell AABBをscreenへ投影。
- screen AABBに対応するpyramid levelを選ぶ。
- cellのview depth rangeとpyramid min/max rangeが交差すれば visible mark。

候補出力:

```hlsl
RWBuffer<uint> RWFspCellVisibleFrameBuffer;
RWBuffer<uint> RWFspCellDepthHintBuffer;
```

メリット:

- pixel数ではなくcell数スケールに近づく。
- global atomicをほぼ不要にできる。
- cascadeごとの処理はcell dispatchで自然に扱える。

リスク:

- projected AABB が保守的すぎると過剰markが増える。
- depth discontinuityや薄いgeometryに対してmin/maxが過剰判定しやすい。
- 実装規模がtile slice案より大きい。

### Model B に任せる作業プロンプト

```
あなたは sample_project_agent リポジトリで InstantRDV の FSP visible surface 抽出の将来方式を設計/試作する担当です。

対象は Cell-driven + Depth Pyramid Min/Max 方式です。
これは第一案の Tile Slice Mask とは独立した検証ブランチとして扱ってください。

参照する計画書:
- fsp_visible_surface_optimization_plan.md

主な対象ファイル:
- ngl_v001/ngl/shader/instant_rdv/fsp/
- ngl_v001/ngl/shader/instant_rdv/instant_rdv_util.hlsli
- ngl_v001/ngl/src/render/app/instant_rdv/instant_rdv.cpp
- ngl_v001/ngl/include/render/app/instant_rdv/instant_rdv.h

設計要件:
1. full-res depthからmin/max depth pyramidを作るpass構成を設計する。
2. FSP cell AABBをscreenへ投影し、depth pyramid rangeと交差判定するpassを設計する。
3. Reverse-Z、sky depth、view-z変換の扱いを明記する。
4. 既存finalize passに接続できるよう、出力は `RWFspCellVisibleFrameBuffer` / `RWFspCellDepthHintBuffer` を優先する。
5. 取り逃しを避け、過剰markを許容する保守的判定にする。
6. Debug UIで他方式と切り替え可能にする設計にする。
7. .hlsl/.hlsliはUTF-8 BOMなしを維持する。

まずは実装前に、必要なbuffer/texture、pass順、projection math、pyramid level選択、想定リスクを短い設計メモとしてまとめてください。
その後、最小実装に進んでください。
```

## 共通の評価指標

- GPU時間
  - FSP visible surface total
  - 各sub pass
  - FSP pre-update/updateへの波及
- 品質/coverage
  - visible surface cell count
  - active probe count
  - debug visualizationでの欠落有無
  - probe lightingのちらつき/抜け
- 安定性
  - camera移動時のprobe pop
  - depth discontinuity付近の過剰/欠落
  - cascade境界付近の挙動

## 実装時の注意

- `ngl_v001/ngl/shader/instant_rdv/` 配下の `.hlsl` / `.hlsli` は UTF-8 BOMなしを維持する。
- 新規shaderは先頭にファイル名と説明コメントを置く。
- 主要ブロックごとに処理意図コメントを書く。
- CPU側 `SetView` 名と shader側 resource 宣言名の一致を確認する。
- Producer/Consumer間の UAV barrier を明示する。
- Legacy 1-pass は比較/切り分け用に残す。
- Debug UI切替時に同じframe内でbuffer状態が破綻しないよう、各方式の出力先を共通finalize形式へ揃える。
