#ifndef TOML11_VERSION_HPP
#define TOML11_VERSION_HPP

#define TOML11_VERSION_MAJOR 4
#define TOML11_VERSION_MINOR 2
#define TOML11_VERSION_PATCH 0

#ifndef __cplusplus
#    error "__cplusplus is not defined"
#endif

// Since MSVC does not define `__cplusplus` correctly unless you pass
// `/Zc:__cplusplus` when compiling, the workaround macros are added.
//
// The value of `__cplusplus` macro is defined in the C++ standard spec, but
// MSVC ignores the value, maybe because of backward compatibility. Instead,
// MSVC defines _MSVC_LANG that has the same value as __cplusplus defined in
// the C++ standard. So we check if _MSVC_LANG is defined before using `__cplusplus`.
//
// FYI: https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
//      https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170
//

#if defined(_MSVC_LANG) && defined(_MSC_VER) && 190024210 <= _MSC_FULL_VER
#  define TOML11_CPLUSPLUS_STANDARD_VERSION _MSVC_LANG
#else
#  define TOML11_CPLUSPLUS_STANDARD_VERSION __cplusplus
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION < 201103L
#    error "toml11 requires C++11 or later."
#endif

#if ! defined(__has_include)
#  define __has_include(x) 0
#endif

#if ! defined(__has_cpp_attribute)
#  define __has_cpp_attribute(x) 0
#endif

#if ! defined(__has_builtin)
#  define __has_builtin(x) 0
#endif

// hard to remember

#ifndef TOML11_CXX14_VALUE
#define TOML11_CXX14_VALUE 201402L
#endif//TOML11_CXX14_VALUE

#ifndef TOML11_CXX17_VALUE
#define TOML11_CXX17_VALUE 201703L
#endif//TOML11_CXX17_VALUE

#ifndef TOML11_CXX20_VALUE
#define TOML11_CXX20_VALUE 202002L
#endif//TOML11_CXX20_VALUE

#if defined(__cpp_char8_t)
#  if __cpp_char8_t >= 201811L
#    define TOML11_HAS_CHAR8_T 1
#  endif
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<string_view>)
#    define TOML11_HAS_STRING_VIEW 1
#  endif
#endif

#ifndef TOML11_DISABLE_STD_FILESYSTEM
#  if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#    if __has_include(<filesystem>)
#      define TOML11_HAS_FILESYSTEM 1
#    endif
#  endif
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<optional>)
#    define TOML11_HAS_OPTIONAL 1
#  endif
#endif

#if defined(TOML11_COMPILE_SOURCES)
#  define TOML11_INLINE
#else
#  define TOML11_INLINE inline
#endif

namespace toml
{

inline const char* license_notice() noexcept
{
    return R"(The MIT License (MIT)

Copyright (c) 2017-now Toru Niina

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.)";
}

} // toml
#endif // TOML11_VERSION_HPP
