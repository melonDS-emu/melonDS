# Immediate Input Edge Overlay + Direct AltForm Transform - 実装メモ

## 目的

MPH の内部入力 struct (`player+0x464`) に対して、エミュレータ側で生成した
pressed / released edge を注入する。

ゲーム側の入力経路では、NDS KEYINPUT を読んで pressed/released を計算するため、
エミュレータの input capture タイミングと NDS RunFrame の実行タイミングの差が
1フレームの遅延として現れることがある。

このパッチは、ゲームの input poll が済んだ直後、かつ action consumer が
input struct を読む直前の ARM9 命令にフックし、エミュレータ側で再計算した
pressed/released edge を割り込みで書き込むことで、その遅延を詰める。

機能は独立した2つの設定で制御される。

| 設定 | 対象アクション | 方式 |
| --- | --- | --- |
| Immediate Input Edge Overlay | Fire / Jump / Zoom / 移動 (前後左右) | binding field から low 16bit 読み取り |
| Direct Alt-Form Transform | Morph / AltForm (変形・解除) | 合成 binding を `player+0x3A0` に書き込み |

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
action consumer entry (02024174 etc.)
  ↓
morph check / fire gate / jump gate … が input struct を読んで行動判定
  ↓
TransformRequest (02016338) / fire / jump / zoom 実行
```

### overlay の流れ

```
hook 発火 (action consumer エントリー直前)
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

フックアドレスとして **action consumer エントリー** を使う。
これは input poll の後、全アクションチェック（morph / fire / jump…）の前に実行される。

| Version | Action consumer (hook addr) |
| --- | ---: |
| JP1_0 | `0x02024174` |
| JP1_1 | `0x02024174` |
| US1_0 | `0x02024198` |
| US1_1 | `0x02024198` |
| EU1_0 | `0x02024190` |
| EU1_1 | `0x02024198` |
| KR1_0 | `0x0200F6DCu` |

---

## 対応アクションと binding offset

ゲームは操作プリセット (Touch R / Touch L / Dual R / Dual L) に応じて
`player+0x368` 以降の binding table を書き換える。

各 entry は u32 で、**lower 16 bits = 物理 NDS ボタン bit マスク**。
upper 16 bits は input helper の動作 selector (0x40000 = pressed-edge 等)。
overlay 側では **`binding & 0xFFFF` だけ**を input struct に書く。

### Immediate Input Edge Overlay 対象

| Offset | アクション | 種別 | emulator IB_ |
| ---: | --- | --- | --- |
| `+0x368` | Move left | held / current 中心 | `IB_MOVE_L` |
| `+0x36C` | Move right | held / current 中心 | `IB_MOVE_R` |
| `+0x370` | Move forward (up) | held / current 中心 | `IB_MOVE_F` |
| `+0x374` | Move backward (down) | held / current 中心 | `IB_MOVE_B` |
| `+0x398` | Fire | pressed edge 重要 | `IB_SHOOT` |
| `+0x39C` | Jump (normal binding path) | pressed edge 重要 | `IB_JUMP` |
| `+0x3E0` | Zoom | pressed edge 必須 | `IB_ZOOM` |

### Direct Alt-Form Transform 対象

| Offset | アクション | 合成値 | emulator IB_ |
| ---: | --- | --- | --- |
| `+0x3A0` | Morph / Alt-form | `0x00040004` (Select) | `IB_MORPH` |

`player+0x3A0` は全プリセットで `0x00000000` のため、
ゲーム本来の binding が存在しない。
代わりに `Consts::MORPH_SYNTHETIC_BINDING = 0x00040004` を game join 時と
各 morph press 時に書き込む。

---

## Morph 合成 binding の詳細

### なぜ player+0x3A0 は常に 0 か

モーフはボタンバインディングを持たず、タッチ入力をフォールバックで検出する仕組み。
そのため全プリセットで `+0x3A0 = 0x00000000`。

### 合成 binding の値

```
MORPH_SYNTHETIC_BINDING = 0x00040004
  = 0x00040000 (pressed-edge selector)
  | 0x00000004 (Select button NDS bit)
```

`0x40000` selector は morph bomb (`0x00040200`) 等と同じ pressed-edge スタイル。

`02028EE8` の処理:
1. `ands r2,r1,#0x40000` → binding の pressed-edge selector を確認
2. `ldrh r2,[r0,#+0x4]` → `input+0x04` (pressed field) を無条件ロード
3. `ands r2,r1,r2` → `binding & pressed_field`
4. non-zero なら return 1 → `02016338(player, 0)` 呼び出し

overlay が 0x0004 を `input.pressed` に注入 → `0x00040004 & 0x0004 = 0x0004 ≠ 0` → morph 発動。

### Dual R/L モードの衝突

Dual R / Dual L では zoom binding (`+0x3E0`) = `0x00000004` (Select)。
これが `MORPH_SYNTHETIC_BIT = 0x0004` と一致するため、
Select を morph 合成 binding に使うと zoom 操作でも morph が発動してしまう。

**対処**: `zoomMask == MORPH_SYNTHETIC_BIT` を runtime で検出し、
衝突時は `player+0x3A0` への書き込みをスキップして **タッチ偽装 fallback** に切り替える。

| プリセット | zoom binding | 衝突 | Direct AltForm 動作 |
| --- | ---: | --- | --- |
| Touch R | `0x0100` (R) | なし | binding injection |
| Touch L | `0x0200` (L) | なし | binding injection |
| Dual R | `0x0004` (Select) | あり | タッチ偽装 fallback |
| Dual L | `0x0004` (Select) | あり | タッチ偽装 fallback |

タッチ偽装でもモーフ自体は機能する。ユーザーには透過的。

### 定数の定義場所

`MelonPrimeInternal.h` → `MelonPrime::Consts` 名前空間:

```cpp
constexpr uint32_t MORPH_SYNTHETIC_BINDING = 0x00040004u;
constexpr uint16_t MORPH_SYNTHETIC_BIT     = 0x0004u;
```

---

## Morph のフロー (Touch R/L, Direct AltForm ON)

```
RunFrameHook:
  IsPressed(IB_MORPH) → HandleRareMorph()
    zoomMask(+0x3E0) != 0x0004 → 衝突なし
    Write32(player+0x3A0, 0x00040004)   ← 合成 binding を書き込む
    return                               ← FrameAdvanceTwice 不要

RunFrame:
  input poll → player+0x464 更新
  action consumer hook 発火 (02024174 etc.)
    ImmediateInputEdgeOverlay_DispatchCheck():
      morphMask = Read(player+0x3A0) & 0xFFFF = 0x0004
      zoomMask (outer) = Read(player+0x3E0) & 0xFFFF ≠ 0x0004
      rawMorphMask != zoomMask → morphMask = 0x0004
      IB_MORPH in m_input.down → injectedHeld |= 0x0004
      prevInjectedHeld = 0 (初回) → injectedPressed = 0x0004
      Write input.pressed |= 0x0004 (managed bit 置換)

  morph check (02023B38):
    ldr r1,[player+0x3A0] = 0x00040004
    bl 02028EE8(input, 0x00040004):
      0x00040004 & input.pressed(0x0004) = 0x0004 ≠ 0 → return 1
    bne → 02016338(player, 0) 呼び出し   ← TransformRequest!
      全ステートガード確認
      hunter-specific 処理 (Weavel Halfturret 等)
      player+0x4C4 bit11 set → morphing 開始
```

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

---

## managedMask の役割

managedMask = 今回 overlay が管理する全 NDS ボタン bit の OR。

```
managedMask = morphMask                              (DirectAltFormTransform)
            | movLMask | movRMask | movFMask | movBMask   (ImmediateInputEdgeOverlay)
            | fireMask | jumpMask | zoomMask              (ImmediateInputEdgeOverlay)
```

2つの機能は独立して動作するが、managed bit は統合して管理される。
一方の機能が OFF でも、有効な機能のみが managedMask に寄与する。

---

## jump の特記事項

JP1_0 では jump 判定が2段構え。

```
020253A0  r1 = 0x90000            ; quick-repress / double-tap selector
020253A8  bl 02028EE8
020253B0  if true → jump

020253B4  r1 = [player+0x39C]    ; 通常 jump binding
020253BC  bl 02028EE8
020253C4  if false → no jump
```

物理ジャンプボタンの即時化には `player+0x39C` 経路で十分。

---

## 有効化条件 (DispatchCheck 内ガード)

以下のすべてが満たされた時のみ overlay を実行する。

```
doOverlay (m_enableImmediateInputEdgeOverlay) OR doMorph (m_enableDirectAltFormTransform) が true
BIT_IN_GAME_INIT が立っている
BIT_LAST_FOCUSED が立っている
isStylusMode == false
AIMBLK_NOT_IN_GAME ビットが立っていない
```

---

## state リセット

`m_immediateOverlayPrevHeld` は以下のタイミングでゼロクリアする。

| タイミング | 場所 |
| --- | --- |
| emu start / reset | `OnEmuStart()` |
| ゲーム離脱時 (BIT_IN_GAME_INIT クリア) | `RunFrameHook()` |
| game join 初期化 | `HandleGameJoinInit()` |

---

## 設定キー

| キー | 型 | デフォルト | 説明 |
| --- | --- | --- | --- |
| `Metroid.Input.Enable.ImmediateInputEdgeOverlay` | bool | false | Fire/Jump/Zoom/移動 overlay |
| `Metroid.Input.Enable.DirectAltFormTransform` | bool | false | Morph 合成 binding + overlay |

両機能は独立して ON/OFF できる。

---

## 関連ファイル

```
MelonPrimePatchImmediateInputEdgeOverlay.inc   本体 (MelonPrimeGameInput.cpp に unity include)
MelonPrimeInternal.h                           MORPH_SYNTHETIC_BINDING / MORPH_SYNTHETIC_BIT 定数
MelonPrime.h                                   m_immediateOverlayPrevHeld / m_enableDirectAltFormTransform
MelonPrime.cpp                                 ReloadConfigFlags / HandleGameJoinInit binding 書き込み
MelonPrimeInGame.cpp                           HandleRareMorph (binding injection + fallback)
MelonPrimeGameInput.cpp                        .inc の include 場所
MelonPrimeArm9Hook.cpp                         Dispatch_ImmediateInputEdgeOverlay 登録
MelonPrimeArm9InstructionHook.inc              ARM9InstructionHookMaxAddresses = 8
MelonPrimeDef.h                                CfgKey::ImmediateInputEdgeOverlay / DirectAltFormTransform
Config.cpp                                     DefaultBools エントリ
InputConfig/MelonPrimeInputConfig.ui           チェックボックス / 説明ラベル
InputConfig/MelonPrimeInputConfig.cpp          ロード + enabled 制御
InputConfig/MelonPrimeInputConfigConfig.cpp    セーブ
```

---

## ARM9InstructionHookMaxAddresses の変更

`MelonPrimeArm9InstructionHook.inc` の `ARM9InstructionHookMaxAddresses` を 4 → 8 に拡張。

developer build でのフック数 (JP1_0 例):

| フック | アドレス数 |
| --- | --- |
| NativeAimDelta | 1 |
| ShadowFreeze | 2 |
| NoxusBlade (dev only) | 1 |
| ImmediateInputEdgeOverlay / DirectAltFormTransform | 1 (共有) |
| 合計 | **5** |

---

## 未対応・拡張候補

### Morph の player+0x3A0 = 0 の根本理由

モーフはボタン binding を持たずタッチ入力依存。全プリセットで `+0x3A0 = 0x00000000`。
Direct AltForm Transform は合成 binding を書き込むことで binding injection 経路を使えるようにしている。

### Aim 方向

`+0x388`～`+0x394` は既存 aim 入力経路 (ProcessAimInputMouse / NativeAimDeltaHook) と競合するため対象外。

### Hunter 別 alt attack

各 hunter 専用 binding が `+0x3C8`～`+0x3DC` にある (Noxus/Spire/Sylux/Weavel)。
pressed edge 追加で効果が出やすいが個別検証が必要。

### double-tap / touch jump

`0x90000` 経路 (`input+0x0A`, `input+0x34 bit5`) は未対応。

---

## 調査元ドキュメント

```
C:\Users\Admin\Documents\git\mphCodex\mnt\data\mphAnalysis\
  Input-Direct-Injection-Investigation\#2\
    2_Input-Emulator-Side-Pressed-Overlay-Investigation-AllVersions.md
  Input-Direct-Injection-Investigation\#3\
    Input-Action-Bindings-Movement-Jump-Zoom-Investigation-AllVersions.md
    Input-Action-Bindings-Preset-Dump-Reference-Update-JP1_0.md
  AltFormDirect\
    AltForm-Direct-Transform-Investigation-JP1_0.md
  Player-Control-Preset-Dumps-JP1_0.md
```
