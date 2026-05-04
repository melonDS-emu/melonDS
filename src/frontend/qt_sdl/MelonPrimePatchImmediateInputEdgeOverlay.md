# Immediate Input Edge Overlay - 実装メモ

## 目的

MPH の内部入力 struct (`player+0x464`) に対して、エミュレータ側で生成した
pressed / released edge を注入する。

ゲーム側の入力経路では、NDS KEYINPUT を読んで pressed/released を計算するため、
エミュレータの input capture タイミングと NDS RunFrame の実行タイミングの差が
1フレームの遅延として現れることがある。

このパッチは、ゲームの input poll が済んだ直後、かつ action consumer が
input struct を読む直前の ARM9 命令にフックし、エミュレータ側で再計算した
pressed/released edge を割り込みで書き込むことで、その遅延を詰める。

対応アクション: **Fire / Jump / Zoom / 移動 (前後左右)**。

Aim 方向は対象外（既存の aim 入力経路で処理される）。

---

## 仕組み

### ゲーム側の通常経路

```
NDS KEYINPUT (ハードウェア)
  ↓
ゲームの input poll 関数 (02029754 etc.)
  ↓
player+0x464 の input struct を更新
  ↓  ← ここに hook を挟む
fire gate / action consumer が input struct を読んで行動判定
```

### overlay の流れ

```
hook 発火 (fire gate 命令直前)
  ↓
player の binding field (player+0x368 etc.) から low 16bit を読む
  ↓
m_input.down から emulator-side held 状態を作る
  ↓
前フレームの prevHeld と比較して pressed / released edge を生成
  ↓
input struct の managed bit だけを置換 (managed 外は game の poll 結果のまま)
  ↓
prevHeld を更新
```

### なぜ KEYINPUT 合成だけでは不十分か

NDS KEYINPUT は「今押されているか」しか持たない。
`pressed` / `released` edge はゲーム側の poll 関数が前回値との差分から計算するため、
poll 完了後に KEYINPUT を変えても **同フレームの pressed** にはならない。

このパッチは poll 完了後の input struct に直接 edge を書くため、
同フレームの行動判定に即座に反映できる。

---

## hook タイミング

フックアドレスとして fire gate 先頭命令を使う。
ゲームの input poll は fire gate より前に完了しているため、
フック発火時点で input struct はすでに最新状態になっている。

| Version | Fire gate (hook addr) |
| --- | ---: |
| JP1_0 | `0x020271E0` |
| JP1_1 | `0x020271E0` |
| US1_0 | `0x02027204` |
| US1_1 | `0x02027204` |
| EU1_0 | `0x020271FC` |
| EU1_1 | `0x02027204` |
| KR1_0 | `0x0200BD28` |

フックは `MelonPrimeArm9Hook` の共有ディスパッチャが管理する。
`Dispatch_ImmediateInputEdgeOverlay` ビットで識別される。

---

## 対応アクションと binding offset

ゲームは操作プリセット (Touch R / Touch L / Dual R / Dual L) に応じて
`player+0x368` 以降の binding table を書き換える。

各 entry は u32 で、**lower 16 bits = 物理 NDS ボタン bit マスク**、
upper 16 bits = input helper の動作 selector (0x40000 = pressed edge 等)。

overlay 側では **`binding & 0xFFFF` だけ**を input struct に書く。
upper bits をそのまま入れてはいけない。

| Offset | アクション | 種別 | emulator IB_ |
| ---: | --- | --- | --- |
| `+0x368` | Move left | held / current 中心 | `IB_MOVE_L` |
| `+0x36C` | Move right | held / current 中心 | `IB_MOVE_R` |
| `+0x370` | Move forward (up) | held / current 中心 | `IB_MOVE_F` |
| `+0x374` | Move backward (down) | held / current 中心 | `IB_MOVE_B` |
| `+0x398` | Fire | pressed edge 重要 | `IB_SHOOT` |
| `+0x39C` | Jump (normal binding path) | pressed edge 重要 | `IB_JUMP` |
| `+0x3E0` | Zoom | pressed edge 必須 | `IB_ZOOM` |

### プリセット別 binding 例 (JP1_0 観測値)

| Offset | Touch R | Touch L | Dual R | Dual L |
| ---: | ---: | ---: | ---: | ---: |
| `+0x368` move left | `0x0020` | `0x0800` | `0x0020` | `0x0800` |
| `+0x36C` move right | `0x0010` | `0x0001` | `0x0010` | `0x0001` |
| `+0x370` move fwd | `0x0040` | `0x0400` | `0x0040` | `0x0400` |
| `+0x374` move back | `0x0080` | `0x0002` | `0x0080` | `0x0002` |
| `+0x398` fire | (weapon依存) | (weapon依存) | (weapon依存) | (weapon依存) |
| `+0x39C` jump | `0x0C03` | `0x00F0` | `0x0100` | `0x0200` |
| `+0x3E0` zoom | `0x0100` | `0x0200` | `0x0004` | `0x0004` |

Jump の binding はプリセットによって A/B/X/Y cluster、D-pad all、R、L と
**大きく変わる**。固定ビットを決め打ちするのは危険で、必ず binding field から読む。

---

## input struct (player+0x464)

| Offset | Type | 名前 | 意味 |
| ---: | --- | --- | --- |
| `+0x00` | u16 | buttons_current | 押下中 / held |
| `+0x02` | u16 | buttons_previous | 前回 |
| `+0x04` | u16 | buttons_pressed | 押した瞬間 / pressed edge |
| `+0x06` | u16 | buttons_prevheld_like | 継続・前回系 |
| `+0x08` | u16 | buttons_released | 離した瞬間 / released edge |
| `+0x0A` | u16 | buttons_quick_repress | quick re-press / double-tap 系 |

overlay では `+0x00 / +0x04 / +0x08` の3フィールドだけを書き換える。
`+0x02 / +0x06 / +0x0A` は触らない。

---

## managedMask の役割

managedMask = 今回 overlay が管理する全 NDS ボタン bit の OR。

```
managedMask = fireMask | jumpMask | zoomMask | movLMask | movRMask | movFMask | movBMask
```

managed bit については game の poll 結果を捨てて emulator side の値で置換する。

```
new_current  = (game_current  & ~managedMask) | injectedHeld
new_pressed  = (game_pressed  & ~managedMask) | injectedPressed
new_released = (game_released & ~managedMask) | injectedReleased
```

managedMask 外のビットは一切変えない。例えば Start / Select / メニューボタン等は
ゲームの poll がそのまま残る。

### edge 生成

```
injectedPressed  = injectedHeld & ~prevInjectedHeld
injectedReleased = ~injectedHeld & prevInjectedHeld
```

`m_immediateOverlayPrevHeld` が全アクション共通の前フレーム held 状態を保持する。
managedMask は毎フレーム binding field から再計算されるため、
prevHeld との AND を取ってから edge を計算する。

---

## jump の特記事項

### 通常 jump と double-tap jump は別経路

JP1_0 では jump 判定が2段構えになっている。

```
020253A0  r1 = 0x90000            ; quick-repress / double-tap selector
020253A8  bl 02028EE8             ; input helper
020253B0  if true → jump

020253B4  r1 = [player+0x39C]    ; 通常 jump binding
020253BC  bl 02028EE8
020253C4  if false → no jump
```

`0x90000 = 0x80000 | 0x10000` は `input+0x0A (buttons_quick_repress)` 系を読む。
このパッチは `player+0x39C` 経路のみを対象とする。

物理ジャンプボタンの即時化としてはこれで十分。
double-tap / touch jump を完全制御したい場合は `input+0x0A` と
`input+0x34 bit5 (touch_repress)` への追加対応が必要になるが、現状は未対応。

---

## 有効化条件 (DispatchCheck 内ガード)

以下のすべてが満たされた時のみ overlay を実行する。

```
m_enableImmediateInputEdgeOverlay == true
BIT_IN_GAME_INIT が立っている
BIT_LAST_FOCUSED が立っている (フォーカス有り)
isStylusMode == false
AIMBLK_NOT_IN_GAME ビットが立っていない
```

スタイラスモードでは touchscreen を aim 入力に使うため除外する。
フォーカスが外れた場合は emulator 側の m_input.down がすでにクリアされるが、
二重防止として BIT_LAST_FOCUSED でも gate をかける。

---

## state リセット

`m_immediateOverlayPrevHeld` は以下のタイミングでゼロクリアする。

| タイミング | 場所 |
| --- | --- |
| emu start / reset | `OnEmuStart()` |
| ゲーム離脱時 (BIT_IN_GAME_INIT クリア) | `RunFrameHook()` の isInGame 喪失 branch |
| game join 初期化 | `HandleGameJoinInit()` の先頭 |

binding field はプリセット変更でも自動追従する (毎フレーム読み直すため)。

---

## 設定キー

| キー | 型 | デフォルト | 説明 |
| --- | --- | --- | --- |
| `Metroid.Input.Enable.ImmediateInputEdgeOverlay` | bool | false | 機能の有効/無効 |

`MelonPrimeDef.h` の `CfgKey::ImmediateInputEdgeOverlay` でアクセスする。
`ReloadConfigFlags()` で `m_enableImmediateInputEdgeOverlay` にキャッシュされる。

---

## 関連ファイル

```
MelonPrimePatchImmediateInputEdgeOverlay.inc   本体 (MelonPrimeGameInput.cpp に unity include)
MelonPrime.h                                   メンバー宣言 + m_immediateOverlayPrevHeld
MelonPrime.cpp                                 ReloadConfigFlags / リセット処理
MelonPrimeGameInput.cpp                        .inc の include 場所
MelonPrimeArm9Hook.cpp                         Dispatch_ImmediateInputEdgeOverlay 登録
MelonPrimeArm9InstructionHook.inc              ARM9InstructionHookMaxAddresses = 8 へ拡張
MelonPrimeDef.h                                CfgKey::ImmediateInputEdgeOverlay
Config.cpp                                     DefaultBools エントリ
InputConfig/MelonPrimeInputConfig.ui           チェックボックス / 説明ラベル
InputConfig/MelonPrimeInputConfig.cpp          ロード + enabled 制御
InputConfig/MelonPrimeInputConfigConfig.cpp    セーブ
```

---

## ARM9InstructionHookMaxAddresses の変更

このパッチ追加に伴い、`MelonPrimeArm9InstructionHook.inc` の
`ARM9InstructionHookMaxAddresses` を 4 → 8 に拡張した。

developer build でのフック数計算 (JP1_0 例):

| フック | アドレス数 |
| --- | --- |
| NativeAimDelta | 1 |
| ShadowFreeze | 2 |
| NoxusBlade (dev only) | 1 |
| ImmediateInputEdgeOverlay | 1 |
| 合計 | **5** |

変更前は max=4 で developer build がオーバーフローしていた。

---

## 未対応・拡張候補

### Aim 方向

`+0x388`～`+0x394` (aim left/right/up/down) は現状対象外。
既存の aim 入力経路 (ProcessAimInputMouse / NativeAimDeltaHook) と競合するため
慎重な検証が必要。

### Hunter 別 alt attack

各 hunter 専用の alt attack binding が `+0x3C8`～`+0x3DC` にある。

| Offset | Hunter | 種別 |
| ---: | --- | --- |
| `+0x3C8` | Noxus Vhoscythe | held + pressed 両用 |
| `+0x3D0` | Spire Dialanche | pressed edge |
| `+0x3D4` | Sylux Triskelion | pressed edge |
| `+0x3DC` | Weavel Halfturret | pressed edge |

いずれも pressed edge 追加で効果が出やすいが、個別検証が必要。

### Dual mode の secondary direction binding

Dual R / Dual L では `+0x378`～`+0x384` に secondary direction binding がある。
現状は `+0x368`～`+0x374` の primary direction だけを対象にしている。
secondary が movement consumer から読まれているかどうか未確認。

### double-tap / touch jump

前述の通り `0x90000` 経路 (`input+0x0A`, `input+0x34 bit5`) は未対応。

---

## 調査元ドキュメント

```
C:\Users\Admin\Documents\git\mphCodex\mnt\data\mphAnalysis\
  Input-Direct-Injection-Investigation\#2\
    2_Input-Emulator-Side-Pressed-Overlay-Investigation-AllVersions.md
  Input-Direct-Injection-Investigation\#3\
    Input-Action-Bindings-Movement-Jump-Zoom-Investigation-AllVersions.md
    Input-Action-Bindings-Preset-Dump-Reference-Update-JP1_0.md
    020253A0-jump-input-gate-JP1_0.md
    02025460-movement-direction-checks-JP1_0.md
    02025CFC-zoom-input-gate-JP1_0.md
    02023068-altform-special-input-gates-JP1_0.md
    02023B30-morph-altform-input-gate-JP1_0.md
```
