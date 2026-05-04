# Build

## Build Notes
- Main feature flags in `src/frontend/qt_sdl/CMakeLists.txt`:
  - `MELONPRIME_DS`
  - `MELONPRIME_CUSTOM_HUD`
- MelonPrimeDS development builds should configure with `MELONPRIME_ENABLE_DEVELOPER_FEATURES=ON`
  - This enables the `DEVELOPER ONLY` settings section for local development and patch verification
  - Release/distribution builds should explicitly configure this option `OFF`
- `MelonPrimeHudRender.cpp` and `InputConfig/MelonPrimeInputConfig.cpp` are explicitly built as part of the frontend
  - `MelonPrimeHudRender*.inc` files are unity include fragments pulled in by `MelonPrimeHudRender.cpp`; do not add them to `CMakeLists.txt`
- `MelonPrimeHudConfigOnScreen.cpp` is a unity-build include (pulled in by `MelonPrimeHudRender.cpp`); do not add it to `CMakeLists.txt`
  - Its `MelonPrimeHudConfigOnScreen*.inc` fragments are also unity include fragments; do not add those to `CMakeLists.txt` either
- `MelonPrimeHudScreenCpp*.inc` files are unity include fragments pulled into `Screen.cpp`; do not add them to `CMakeLists.txt`
- The project has been built on Windows and via MinGW cross-compilation from WSL
- `vcpkg/` is used for dependencies in this repo setup

## Windows Build Command
Windows-only AI build command. Do not rebuild, bootstrap, or reinstall `vcpkg/` unless the user explicitly asks for it.

Use MSYS bash and the existing CMake build preset. Put `/mingw64/bin` first in `PATH`; Qt's `moc.exe` depends on MinGW DLLs such as `libgcc_s_seh-1.dll` and `libstdc++-6.dll`, and can fail with `Process failed with return value 3221225781` if they are not discoverable.

For MelonPrimeDS development, run CMake configure before building and pass `-DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON`. Use `-S . -B build/release-mingw-x86_64` for configure because the MinGW configure preset can be disabled when launched from a Windows-hosted shell.

**Important**: Claude Code's Bash tool runs in bash (not PowerShell). The `& '...'` syntax is PowerShell-only and fails in bash with `syntax error near unexpected token '&'`. Always wrap the command with `powershell.exe -Command "..."`.

Also include `/usr/bin` in PATH so standard utilities like `tail` are available inside the MSYS bash session.

For verbose output:

```bash
powershell.exe -Command "& 'C:\msys64\usr\bin\bash.exe' -lc \"cd /c/Users/Admin/Documents/git/melonPrimeDS && export PATH='/mingw64/bin:/usr/bin:/c/Program Files/Python312:/c/Program Files/Python312/Scripts:/c/Users/Admin/Documents/git/melonPrimeDS/build/release-mingw-x86_64/vcpkg_installed/x64-mingw-static-release/tools/Qt6/bin:/c/Users/Admin/Documents/git/melonPrimeDS/build/release-mingw-x86_64/vcpkg_installed/x64-mingw-static-release/bin:\$PATH'; /mingw64/bin/cmake.exe -S . -B build/release-mingw-x86_64 -DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON && /mingw64/bin/cmake.exe --build --preset=release-mingw-x86_64 --parallel 1 --verbose 2>&1\""
```

For short output:

```bash
powershell.exe -Command "& 'C:\msys64\usr\bin\bash.exe' -lc \"cd /c/Users/Admin/Documents/git/melonPrimeDS && export PATH='/mingw64/bin:/usr/bin:/c/Program Files/Python312:/c/Program Files/Python312/Scripts:/c/Users/Admin/Documents/git/melonPrimeDS/build/release-mingw-x86_64/vcpkg_installed/x64-mingw-static-release/tools/Qt6/bin:/c/Users/Admin/Documents/git/melonPrimeDS/build/release-mingw-x86_64/vcpkg_installed/x64-mingw-static-release/bin:\$PATH'; /mingw64/bin/cmake.exe -S . -B build/release-mingw-x86_64 -DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON && /mingw64/bin/cmake.exe --build --preset=release-mingw-x86_64 --parallel 1 2>&1 | tail -20\""
```

When building manually from the `C:\msys64\mingw64.exe` console:

```bash
export PATH="/c/Program Files/Python312:/c/Program Files/Python312/Scripts:$PATH"
cd /c/Users/Admin/Documents/git/melonPrimeDS
cmake -S . -B build/release-mingw-x86_64 -DMELONPRIME_ENABLE_DEVELOPER_FEATURES=ON
cmake --build --preset=release-mingw-x86_64 --parallel 2
```

Notes:
- Use `powershell.exe -Command "..."` wrapper and escape inner `$PATH` as `\$PATH` so bash receives it unexpanded.
- Include `/usr/bin` in PATH inside the bash session so utilities like `tail` work.
- Avoid unlimited `--parallel`; it can exhaust RAM during compilation and fail with `cc1plus.exe: out of memory allocating 65536 bytes`. Use `--parallel 1` for AI builds, or `--parallel 2` when building manually if memory allows.
- Ninja detects changes by mtime. If files were edited externally and ninja reports `no work to do`, run `touch` on the changed files before rebuilding:
  ```bash
  powershell.exe -Command "& 'C:\msys64\usr\bin\bash.exe' -lc \"touch /c/Users/Admin/Documents/git/melonPrimeDS/src/frontend/qt_sdl/ChangedFile.cpp\""
  ```
