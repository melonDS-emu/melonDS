# Shadow Freeze - MelonPrimeDS implementation reissue

## 目的

Shadow Freeze 修正を MelonPrimeDS 側で扱いやすい形に出し直したもの。

今回は `ARM9Read8/ARM9Write8` で 32bit word を組み立てる実装ではなく、`ARM9Read32/ARM9Write32` が使える前提で整理した。

## 推奨構成

### 本命: Runtime hook no-cave

ファイル:

```text
MelonPrimePatchShadowFreezeRuntimeHook.h
MelonPrimePatchShadowFreezeRuntimeHook.cpp
```

特徴:

```text
code cave 不使用
ARM9 RAM への独自ASM配置なし
ARM9 memory write なし
C++側で full 3D angle を再計算
range = lateral distance
angle = full 3D angle
```

この方式では、Ice Wave の既存処理が lateral range check を通過したあと、最後の `ble miss` 分岐だけを MelonPrimeDS 側で横取りする。

JP1_0 の例:

```text
body:
  decision = 0203E5FC
  hit      = 0203E600
  miss     = 0203E62C

halfturret:
  decision = 0203E734
  hit      = 0203E738
  miss     = 0203E764
```

実行時には次を満たす場合だけ横取りする。

```text
current instruction == 0xDA00000A  // ble miss
config Metroid.BugFix.FixShadowFreeze == true
```

## Integration image

ARM9 の命令実行直前に呼ぶ。

```cpp
uint32_t redirectExecAddr = 0;

if (MelonPrime::ShadowFreezeRuntimeHook_CheckAndRedirect(
        nds,
        config,
        romGroupIndex,
        arm9ExecAddr,
        regs,
        redirectExecAddr))
{
    // この命令を通常実行せず、既存の hit/miss branch 先へ飛ばす。
    SetArm9ExecuteAddress(redirectExecAddr);
    return;
}
```

`regs[15]` が ARM pipeline 済みの R15 しか取れない場所では、こちらを使う。

```cpp
uint32_t redirectExecAddr = 0;

if (MelonPrime::ShadowFreezeRuntimeHook_CheckAndRedirectFromPipelinedR15(
        nds,
        config,
        romGroupIndex,
        regs,
        redirectExecAddr))
{
    SetArm9ExecuteAddress(redirectExecAddr);
    return;
}
```

## Fallback: 32bit word patch

ファイル:

```text
MelonPrimePatchShadowFreezeWordPatch.h
MelonPrimePatchShadowFreezeWordPatch.cpp
```

特徴:

```text
code cave 不使用
ARM9Write32 による命令word patchのみ
range = full 3D distance
angle = full 3D angle
```

これは runtime hook が使えない場合の保険。

config key は衝突防止のため分けている。

```text
Metroid.BugFix.FixShadowFreezeWordPatch
```

`Metroid.BugFix.FixShadowFreeze` の runtime hook と同時使用しない方がよい。

## なぜ Read32/Write32 にしたか

ARM命令、座標、方向ベクトルはいずれも 32bit word 単位で扱うので、`ARM9Read32/ARM9Write32` の方が自然。

```text
beam + 0x70 = Direction Vec3 s32 x/y/z
beam + 0xA0 = Position Vec3 s32 x/y/z
player + 0x1C = Body position Vec3 s32 x/y/z
halfturret + 0x30 = Halfturret position Vec3 s32 x/y/z
```

## 対応バージョン

```text
JP1_0
JP1_1
US1_0
US1_1
EU1_0
EU1_1
KR1_0
```

## 推奨

MelonPrimeDS の標準機能として入れるなら、まず runtime hook no-cave 版を使う。
ただしデフォルトは false にしておき、必要なユーザーだけ有効化する。

```text
Metroid.BugFix.FixShadowFreeze = true
```

runtime hook の組み込み位置が難しい場合だけ、fallback の word patch を使う。

```text
Metroid.BugFix.FixShadowFreezeWordPatch = true
```
