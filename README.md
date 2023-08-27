<p align="center"><img src="https://raw.githubusercontent.com/melonDS-emu/melonDS/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>melonDS</b></h2>
<p align="center">
<a href="http://melonds.kuribo64.net/" alt="melonDS website"><img src="https://img.shields.io/badge/website-melonds.kuribo64.net-%2331352e.svg"></a>
<a href="http://melonds.kuribo64.net/downloads.php" alt="Release: 0.9.5"><img src="https://img.shields.io/badge/release-0.9.5-%235c913b.svg"></a>
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
<a href="https://kiwiirc.com/client/irc.badnik.net/?nick=IRC-Source_?#melonds" alt="IRC channel: #melonds"><img src="https://img.shields.io/badge/IRC%20chat-%23melonds-%23dd2e44.svg"></a>
<br>
<a href="https://github.com/melonDS-emu/melonDS/actions?query=workflow%3A%22CMake+Build+%28Windows+x86-64%29%22+event%3Apush"><img src="https://img.shields.io/github/actions/workflow/status/melonDS-emu/melonDS/build-windows.yml?label=Windows%20x86-64&logo=GitHub&branch=master"></img></a>
<a href="https://github.com/melonDS-emu/melonDS/actions?query=workflow%3A%22CMake+Build+%28Ubuntu+x86-64%29%22+event%3Apush"><img src="https://img.shields.io/github/actions/workflow/status/melonDS-emu/melonDS/build-ubuntu.yml?label=Linux%20x86-64&logo=GitHub"></img></a>
<a href="https://github.com/melonDS-emu/melonDS/actions?query=workflow%3A%22CMake+Build+%28Ubuntu+aarch64%29%22+event%3Apush"><img src="https://img.shields.io/github/actions/workflow/status/melonDS-emu/melonDS/build-ubuntu-aarch64.yml?label=Linux%20ARM64&logo=GitHub"></img></a>
<a href="https://github.com/melonDS-emu/melonDS/actions/workflows/build-macos-universal.yml?query=event%3Apush"><img src="https://img.shields.io/github/actions/workflow/status/melonDS-emu/melonDS/build-macos-universal.yml?label=macOS%20Universal&logo=GitHub"></img></a>
</p>
DS emulator, sorta

The goal is to do things right and fast, akin to blargSNES (but hopefully better). But also to, you know, have a fun challenge :)
<hr>

## How to use

Firmware boot (not direct boot) requires a BIOS/firmware dump from an original DS or DS Lite.
DS firmwares dumped from a DSi or 3DS aren't bootable and only contain configuration data, thus they are only suitable when booting games directly.

### Possible firmware sizes

 * 128KB: DSi/3DS DS-mode firmware (reduced size due to lacking bootcode)
 * 256KB: regular DS firmware
 * 512KB: iQue DS firmware

DS BIOS dumps from a DSi or 3DS can be used with no compatibility issues. DSi BIOS dumps (in DSi mode) are not compatible. Or maybe they are. I don't know.

As for the rest, the interface should be pretty straightforward. If you have a question, don't hesitate to ask, though!

## How to build

### Linux
1. Install dependencies:
   * Ubuntu 22.04: `sudo apt install cmake extra-cmake-modules libcurl4-gnutls-dev libpcap0.8-dev libsdl2-dev qtbase5-dev qtbase5-private-dev qtmultimedia5-dev libslirp-dev libarchive-dev libzstd-dev`
   * Older Ubuntu: `sudo apt install cmake extra-cmake-modules libcurl4-gnutls-dev libpcap0.8-dev libsdl2-dev qt5-default qtbase5-private-dev qtmultimedia5-dev libslirp-dev libarchive-dev libzstd-dev`
   * Arch Linux: `sudo pacman -S base-devel cmake extra-cmake-modules git libpcap sdl2 qt5-base qt5-multimedia libslirp libarchive zstd`
3. Download the melonDS repository and prepare:
   ```bash
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```

3. Compile:
   ```bash
   cmake -B build
   cmake --build build -j$(nproc --all)
   ```

### Windows
1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 MinGW 64-bit** terminal
3. Update the packages using `pacman -Syu` and reopen the terminal if it asks you to
4. Install git to clone the repository
   ```bash
   pacman -S git
   ```
5. Download the melonDS repository and prepare:
   ```bash
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```
#### Dynamic builds (with DLLs)
5. Install dependencies: `pacman -S mingw-w64-x86_64-{cmake,SDL2,toolchain,qt5-base,qt5-svg,qt5-multimedia,qt5-tools,libslirp,libarchive,zstd}`
6. Compile:
   ```bash
   cmake -B build
   cmake --build build
   cd build
   ../tools/msys-dist.sh
   ```
If everything went well, melonDS and the libraries it needs should now be in the `dist` folder.

#### Static builds (without DLLs, standalone executable)
5. Install dependencies: `pacman -S mingw-w64-x86_64-{cmake,SDL2,toolchain,qt5-static,libslirp,libarchive,zstd}`
6. Compile:
   ```bash
   cmake -B build -DBUILD_STATIC=ON -DCMAKE_PREFIX_PATH=/mingw64/qt5-static
   cmake --build build
   ```
If everything went well, melonDS should now be in the `build` folder.

### macOS
1. Install the [Homebrew Package Manager](https://brew.sh)
2. Install dependencies: `brew install git pkg-config cmake sdl2 qt@6 libslirp libarchive zstd`
3. Download the melonDS repository and prepare:
   ```zsh
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```
4. Compile:
   ```zsh
   cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix libarchive)" -DUSE_QT6=ON
   cmake --build build -j$(sysctl -n hw.logicalcpu)
   ```
If everything went well, melonDS.app should now be in the `build` directory.

#### Self-contained app bundle
If you want an app bundle that can be distributed to other computers without needing to install dependencies through Homebrew, you can additionally run `
../tools/mac-bundle.rb melonDS.app` after the build is completed, or add `-DMACOS_BUNDLE_LIBS=ON` to the first CMake command.

## TODO LIST

 * better DSi emulation
 * better OpenGL rendering
 * netplay
 * the impossible quest of pixel-perfect 3D graphics
 * support for rendering screens to separate windows
 * emulating some fancy addons
 * other non-core shit (debugger, graphics viewers, etc)

### TODO LIST FOR LATER (low priority)

 * big-endian compatibility (Wii, etc)
 * LCD refresh time (used by some games for blending effects)
 * any feature you can eventually ask for that isn't outright stupid

## Credits

 * Martin for GBAtek, a good piece of documentation
 * Cydrak for the extra 3D GPU research
 * limittox for the icon
 * All of you comrades who have been testing melonDS, reporting issues, suggesting shit, etc

## Licenses

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

melonDS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

### External
* Images used in the Input Config Dialog - see `src/frontend/qt_sdl/InputConfig/resources/LICENSE.md`
