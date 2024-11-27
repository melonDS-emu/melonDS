#ifndef TOML11_LITERAL_FWD_HPP
#define TOML11_LITERAL_FWD_HPP

#include "../location.hpp"
#include "../types.hpp"
#include "../version.hpp" // IWYU pragma: keep for TOML11_HAS_CHAR8_T

namespace toml
{

namespace detail
{
// implementation
::toml::value literal_internal_impl(location loc);
} // detail

inline namespace literals
{
inline namespace toml_literals
{

::toml::value operator"" _toml(const char* str, std::size_t len);

#if defined(TOML11_HAS_CHAR8_T)
// value of u8"" literal has been changed from char to char8_t and char8_t is
// NOT compatible to char
::toml::value operator"" _toml(const char8_t* str, std::size_t len);
#endif

} // toml_literals
} // literals
} // toml
#endif // TOML11_LITERAL_FWD_HPP
