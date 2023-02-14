<p align="center"><img src="https://raw.githubusercontent.com/vitor251093/KHDays_FM/master/res/icon/melon_128x128.png"></p>
<h2 align="center"><b>Kingdom Hearts 358/2 Days - Melon Mix</b></h2>
melonDS, sorta

The goal of this project is to turn "Kingdom Hearts 358/2 Days" into a playable PC game, with a single screen and controls compatible with a regular controller.

This is basically melonDS, but with some modifications made specifically to improve this specific game experience. Any issues you have with KHDays Final Mix should be reported in this same repository, and not in melonDS repository.
<hr>

## How to use

You need an original copy of "Kingdom Hearts 358/2 Days" in order to dump it into a NDS file. Place that file inside the rom folder, and name it game.nds. Only then you will be able to use the emulator properly.

## How to build

### Linux
1. Install dependencies:
   * Ubuntu 22.04: `sudo apt install cmake extra-cmake-modules libcurl4-gnutls-dev libpcap0.8-dev libsdl2-dev qtbase5-dev qtbase5-private-dev qtmultimedia5-dev libslirp-dev libarchive-dev`
   * Older Ubuntu: `sudo apt install cmake extra-cmake-modules libcurl4-gnutls-dev libpcap0.8-dev libsdl2-dev qt5-default qtbase5-private-dev qtmultimedia5-dev libslirp-dev libarchive-dev`
   * Arch Linux: `sudo pacman -S base-devel cmake extra-cmake-modules git libpcap sdl2 qt5-base qt5-multimedia libslirp libarchive`
3. Download the KHDays FM repository and prepare:
   ```bash
   git clone https://github.com/vitor251093/KHDays_FM
   cd KHDays_FM
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
5. Download the KHDays FM repository and prepare:
   ```bash
   git clone https://github.com/vitor251093/KHDays_FM
   cd KHDays_FM
   ```
#### Dynamic builds (with DLLs)
5. Install dependencies: `pacman -S mingw-w64-x86_64-{cmake,SDL2,toolchain,qt5-base,qt5-svg,qt5-multimedia,libslirp,libarchive}`
6. Compile:
   ```bash
   cmake -B build
   cmake --build build
   cd build
   ../tools/msys-dist.sh
   ```
If everything went well, KHDays FM and the libraries it needs should now be in the `dist` folder.

#### Static builds (without DLLs, standalone executable)
5. Install dependencies: `pacman -S mingw-w64-x86_64-{cmake,SDL2,toolchain,qt5-static,libslirp,libarchive}`
6. Compile:
   ```bash
   cmake -B build -DBUILD_STATIC=ON -DCMAKE_PREFIX_PATH=/mingw64/qt5-static
   cmake --build build
   ```
If everything went well, melonDS should now be in the `build` folder.

### macOS
1. Install the [Homebrew Package Manager](https://brew.sh)
2. Install dependencies: `brew install git pkg-config cmake sdl2 qt@6 libslirp libarchive libepoxy`
3. Download the KHDays FM repository and prepare:
   ```zsh
   git clone https://github.com/vitor251093/KHDays_FM
   cd KHDays_FM
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

## DONE

 * the game camera now can be controlled using an analog stick
 * the app should start with OpenGL rendering and 5x native resolution by default
 * the widescreen hack should be included by default
 * the map from the bottom screen should be cropped and placed in the top screen (when visible)

## PARTIALLY DONE

 * only one screen should be visible at a time, switching automatically according to the needs

## BUGS THAT NEED FIXING

 * for a brief moment, the tutorial window appears as a minimap
 * a brief glitch may occur during the transition between a scene with map and without map
 * There is a glitch between Roxas thoughts and the day counter
 * the minimap isn't properly cropped if the emulator window is in an aspect ratio lower than 16:9

## TODO LIST

 * replace textures that look crispy
 * replace the ingame font
 * intro menu should be properly adapted to look more like the KH 1.5 + 2.5 menu

### TODO LIST FOR LATER (low priority)

 * replace cutscenes with the remastered ones
 * automatically skip tutorials that show how to control the camera
 * remove the ingame menu instructions regarding the game camera

## Credits

 * All people that supported and developed melonDS
 * Michael Lipinski, for the [documentation](https://pdfs.semanticscholar.org/657d/adf4888f6302701095055b0d7a066e42b36f.pdf) regarding the way a NDS works

## Licenses

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

KHDays FM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

### External
* Images used in the Input Config Dialog - see `src/frontend/qt_sdl/InputConfig/resources/LICENSE.md`
