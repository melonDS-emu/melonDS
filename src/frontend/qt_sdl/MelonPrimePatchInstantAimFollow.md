# Instant Aim Follow - developer-only note

## 目的

`Instant Aim Follow` は、Metroid Prime Hunters の `currentAim` が `targetAim` へ少しずつ追従する処理を変更し、即座に追従させるための実験的な patch。

通常のゲーム処理では `currentAim` が `targetAim` へ約 10% ずつ寄るため、マウス操作では遅れのように感じることがある。この patch はその追従処理を短絡し、`currentAim` を `targetAim` に近い状態へ即時反映させる。

## 現在の扱い

この機能は 2026-05-03 時点で developer-only 扱いにした。

理由:

```text
ユーザー向けに公開できる品質まで確認できていない
ゲーム挙動への影響範囲がまだ十分に整理されていない
既存ユーザーの config に true が残っていても有効化させたくない
```

## 今回の変更

UI:

```text
Metroid Settings の Sensitivity から Instant Aim Follow を外した
Metroid Settings の一番下に DEVELOPER ONLY セクションを追加
Instant Aim Follow を DEVELOPER ONLY 内へ移動
通常ビルドではチェックボックスと説明文を disabled にしてグレーアウト表示
開発ビルドでは DEVELOPER ONLY 内のチェックボックスを操作可能にする
```

保存:

```text
通常ビルドでは Metroid.Aim.Enable.InstantAimFollow を保存時に false に丸める
開発ビルドでは UI のチェック状態を保存する
```

実行時:

```text
MelonPrimePatchInstantAimFollow.cpp の kInstantAimFollowAvailable をビルドオプションで切り替える
通常ビルドでは cfg が true でも InstantAimFollow_ApplyOnce は patch を apply しない
開発ビルドでは cfg が true のときだけ InstantAimFollow_ApplyOnce が patch を apply する
すでに apply 済みの場合は RestoreOnce 側へ倒す
```

関連ファイル:

```text
src/frontend/qt_sdl/CMakeLists.txt
InputConfig/MelonPrimeInputConfig.ui
InputConfig/MelonPrimeInputConfig.cpp
InputConfig/MelonPrimeInputConfigConfig.cpp
Config.cpp
MelonPrimePatchInstantAimFollow.cpp
MelonPrimePatchFixNoxusBladePersistence.cpp
```

## 開発ビルドで有効化する方法

`DEVELOPER ONLY` セクション内の実験機能は、通常ビルドでは UI・保存・実行時 patch のすべてで無効化される。

開発時だけ有効化したい場合は、configure 時に次の CMake オプションを渡す。

```text
-DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON
```

MSYS bash からの例:

```bash
cd /c/Users/Admin/Documents/git/melonPrimeDS
/mingw64/bin/cmake.exe -S . -B build/release-mingw-x86_64 -DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON
/mingw64/bin/cmake.exe --build --preset=release-mingw-x86_64 --parallel 1
```

通常ビルドへ戻す場合:

```bash
cd /c/Users/Admin/Documents/git/melonPrimeDS
/mingw64/bin/cmake.exe -S . -B build/release-mingw-x86_64 -DMELONPRIME_ENABLE_DEVELOPER_FEATURES=OFF
/mingw64/bin/cmake.exe --build --preset=release-mingw-x86_64 --parallel 1
```

このオプションは現在、次の developer-only 機能に効く。

```text
Metroid.Aim.Enable.InstantAimFollow
Metroid.BugFix.FixNoxusBladePersistence
```

## 再有効化する場合

再び検証するだけなら、developer-only のまま `MELONPRIME_ENABLE_DEVELOPER_FEATURES=ON` でビルドする。

公開する場合は、UI の disabled 状態、保存時 false 丸め、実行時 gate をどう扱うかを別途決める。

公開する場合は、少なくとも次を確認する。

```text
全 ROM group で patch/revert が安全に動くこと
試合開始、リセット、ROM 切り替え、設定変更時に patch 状態が残らないこと
Native Aim Delta Hook や Aim Smoothing 無効化との組み合わせで破綻しないこと
通常プレイで aim の補間を消すことによる副作用が許容できること
```

## 元に戻す方法

AI が通常公開設定へ戻す場合は、次の差し戻しをまとめて行う。

### 1. 実行時 gate を外す

ファイル:

```text
MelonPrimePatchInstantAimFollow.cpp
```

現在:

```cpp
#ifdef MELONPRIME_ENABLE_DEVELOPER_FEATURES
static constexpr bool kInstantAimFollowAvailable = true;
#else
static constexpr bool kInstantAimFollowAvailable = false;
#endif
```

通常公開設定へ戻す場合は、このビルドオプション gate を削除するか、少なくとも常に `true` にする。

`InstantAimFollow_ApplyOnce` は次の形へ戻す。

```cpp
const bool shouldApply = cfg.GetBool(CfgKey::InstantAimFollow);
```

`kInstantAimFollowAvailable && cfg.GetBool(...)` のままにしない。

### 2. UI の初期値を config から読むように戻す

ファイル:

```text
InputConfig/MelonPrimeInputConfig.cpp
```

現在:

```cpp
ui->cbMetroidEnableInstantAimFollow->setChecked(
    kDeveloperOnlyFeaturesEnabled && instcfg.GetBool("Metroid.Aim.Enable.InstantAimFollow"));
```

戻す:

```cpp
ui->cbMetroidEnableInstantAimFollow->setChecked(
    instcfg.GetBool("Metroid.Aim.Enable.InstantAimFollow"));
```

### 3. UI のグレーアウト固定を解除する

ファイル:

```text
InputConfig/MelonPrimeInputConfig.cpp
```

現在:

```cpp
ui->cbMetroidEnableInstantAimFollow->setEnabled(kDeveloperOnlyFeaturesEnabled && enableAimControls);
ui->lblMetroidInstantAimFollowDesc->setEnabled(kDeveloperOnlyFeaturesEnabled && enableAimControls);
```

戻す:

```cpp
ui->cbMetroidEnableInstantAimFollow->setEnabled(enableAimControls);
ui->lblMetroidInstantAimFollowDesc->setEnabled(enableAimControls);
```

### 4. 保存時に false 固定しない

ファイル:

```text
InputConfig/MelonPrimeInputConfigConfig.cpp
```

現在:

```cpp
instcfg.SetBool(
    "Metroid.Aim.Enable.InstantAimFollow",
    kDeveloperOnlyFeaturesEnabled
        && ui->cbMetroidEnableInstantAimFollow->checkState() == Qt::Checked);
```

戻す:

```cpp
instcfg.SetBool(
    "Metroid.Aim.Enable.InstantAimFollow",
    ui->cbMetroidEnableInstantAimFollow->checkState() == Qt::Checked);
```

### 5. UI 配置を元の Sensitivity セクションへ戻す

ファイル:

```text
InputConfig/MelonPrimeInputConfig.ui
```

`DEVELOPER ONLY` 内にある次の widget を、`sectionSensitivity` の `formSens` 内へ戻す。

```text
cbMetroidEnableInstantAimFollow
lblMetroidInstantAimFollowDesc
```

元の配置は、`lblMetroidNativeAimDeltaHookDesc` の直後、`cbMetroidEnableStylusMode` の直前。

戻す XML の目安:

```xml
<item row="5" column="0" colspan="2">
    <widget class="QCheckBox" name="cbMetroidEnableInstantAimFollow">
        <property name="text"><string>Enable Instant Aim Follow</string></property>
    </widget>
</item>
<item row="6" column="0" colspan="2">
    <widget class="QLabel" name="lblMetroidInstantAimFollowDesc">
        <property name="text"><string>The game normally moves currentAim only about 10% toward targetAim each update, which can feel like mouse lag. This patch makes currentAim follow targetAim instantly.</string></property>
        <property name="wordWrap"><bool>true</bool></property>
        <property name="styleSheet"><string>QLabel { margin-left: 20px; }</string></property>
    </widget>
</item>
```

このとき、`enabled=false` と developer-only toolTip は削除する。

### 6. Developer-only セクションを消す場合

Instant Aim Follow 以外に developer-only 機能がないなら、次も削除する。

```text
InputConfig/MelonPrimeInputConfig.ui:
  btnToggleDeveloperOnly
  sectionDeveloperOnly
  lblMetroidDeveloperOnlyDesc

InputConfig/MelonPrimeInputConfig.cpp:
  setupToggle(ui->btnToggleDeveloperOnly, ...)

InputConfig/MelonPrimeInputConfigConfig.cpp:
  instcfg.SetBool("Metroid.UI.SectionDeveloperOnly", ...)

Config.cpp:
  Instance*.Metroid.UI.SectionDeveloperOnly
```

Developer-only セクションを残す場合でも、Instant Aim Follow の widget 名は重複させない。

### 7. 確認

差し戻し後は UI 生成を含めてビルドする。

```text
cmake --build --preset=release-mingw-x86_64 --parallel 1
```

確認ポイント:

```text
MelonPrimeInputConfig.ui から生成される ui_MelonPrimeInputConfig.h に ODR 警告が出ない
Instant Aim Follow が Sensitivity セクションに表示される
Stylus Mode 有効時は他の aim controls と同様に disabled になる
設定保存後に Metroid.Aim.Enable.InstantAimFollow が UI のチェック状態と一致する
config が true のときだけ InstantAimFollow_ApplyOnce が patch を apply する
```
