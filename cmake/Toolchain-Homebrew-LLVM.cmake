# Toolchain file for building with Homebrew's LLVM on macOS
# This is useful on 10.14 where std::filesystem is not supported.

set(CMAKE_C_COMPILER /usr/local/opt/llvm/bin/clang)
set(CMAKE_CXX_COMPILER /usr/local/opt/llvm/bin/clang++)

add_link_options(-L/usr/local/opt/llvm/lib)

# LLVM in Homebrew is built with latest Xcode which has a newer linker than
# what is bundled in the default install of Xcode Command Line Tools, so we
# override it to prevent it passing flags not supported by the system's ld.
add_link_options(-mlinker-version=450)
