# Building melonDS

* [Linux](#linux)
* [Windows](#windows)
* [macOS](#macos)

## Linux
1. Install dependencies:
   * Ubuntu:
     * All versions: `sudo apt install cmake extra-cmake-modules libcurl4-gnutls-dev libpcap0.8-dev libsdl2-dev libarchive-dev libenet-dev libzstd-dev libfaad-dev`
     * 24.04: `sudo apt install qt6-{base,base-private,multimedia,svg}-dev`
     * 22.04: `sudo apt install qtbase6-dev qtbase6-private-dev qtmultimedia6-dev libqt6svg6-dev`
     * Older versions: `sudo apt install qtbase5-dev qtbase5-private-dev qtmultimedia5-dev libqt5svg5-dev`  
       Also add `-DUSE_QT6=OFF` to the first CMake command below.
   * Fedora: `sudo dnf install gcc-c++ cmake extra-cmake-modules SDL2-devel libarchive-devel enet-devel libzstd-devel faad2-devel qt6-{qtbase,qtbase-private,qtmultimedia,qtsvg}-devel wayland-devel`
   * Arch Linux: `sudo pacman -S base-devel cmake extra-cmake-modules git libpcap sdl2 qt6-{base,multimedia,svg} libarchive enet zstd faad2`
2. Download the melonDS repository and prepare:
   ```bash
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```
3. Compile:
   ```bash
   cmake -B build
   cmake --build build -j$(nproc --all)
   ```

## Windows
1. Install [MSYS2](https://www.msys2.org/)
2. Open the MSYS2 terminal from the Start menu:
   * For x64 systems (most common), use **MSYS2 UCRT64**
   * For ARM64 systems, use **MSYS2 CLANGARM64**
3. Update the packages using `pacman -Syu` and reopen the same terminal if it asks you to
4. Install git and clone the repository
   ```bash
   pacman -S git
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```
5. Install dependencies:  
   Replace `<prefix>` below with `mingw-w64-ucrt-x86_64` on x64 systems, or `mingw-w64-clang-aarch64` on ARM64 systems.
   ```bash
   pacman -S <prefix>-{toolchain,cmake,SDL2,libarchive,enet,zstd,faad2}
   ```
6. Install Qt and configure the build directory
   * Dynamic builds (with DLLs)
     1. Install Qt: `pacman -S <prefix>-{qt6-base,qt6-svg,qt6-multimedia,qt6-svg,qt6-tools}`
     2. Set up the build directory with `cmake -B build`
   * Static builds (without DLLs, standalone executable)
     1. Install Qt: `pacman -S <prefix>-qt5-static`  
        (Note: As of writing, the `qt6-static` package does not work.)
     2. Set up the build directory with `cmake -B build -DBUILD_STATIC=ON -DUSE_QT6=OFF -DCMAKE_PREFIX_PATH=$MSYSTEM_PREFIX/qt5-static`
7. Compile: `cmake --build build`

If everything went well, melonDS should now be in the `build` folder. For dynamic builds, you may need to run melonDS from the MSYS2 terminal in order for it to find the required DLLs.

## macOS
1. Install the [Homebrew Package Manager](https://brew.sh)
2. Install dependencies: `brew install git pkg-config cmake sdl2 qt@6 libarchive enet zstd faad2`
3. Download the melonDS repository and prepare:
   ```zsh
   git clone https://github.com/melonDS-emu/melonDS
   cd melonDS
   ```
4. Compile:
   ```zsh
   cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix libarchive)"
   cmake --build build -j$(sysctl -n hw.logicalcpu)
   ```
If everything went well, melonDS.app should now be in the `build` directory.

### Self-contained app bundle
If you want an app bundle that can be distributed to other computers without needing to install dependencies through Homebrew, you can additionally run `
../tools/mac-libs.rb .` after the build is completed, or add `-DMACOS_BUNDLE_LIBS=ON` to the first CMake command.

## Nix (macOS/Linux)

melonDS provides a Nix flake with support for both macOS and Linux. The [Nix package manager](https://nixos.org) needs to be installed to use it.

* To run melonDS, just type `nix run github:melonDS-emu/melonDS`.
* To get a shell for development, clone the melonDS repository and type `nix develop` in its directory.
