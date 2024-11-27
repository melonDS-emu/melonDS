#ifndef TOML11_COLOR_IMPL_HPP
#define TOML11_COLOR_IMPL_HPP

#include "../fwd/color_fwd.hpp"
#include "../version.hpp"

#include <ostream>

namespace toml
{
namespace color
{
// put ANSI escape sequence to ostream
inline namespace ansi
{

TOML11_INLINE std::ostream& reset(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[00m";}
    return os;
}
TOML11_INLINE std::ostream& bold(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[01m";}
    return os;
}
TOML11_INLINE std::ostream& grey(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[30m";}
    return os;
}
TOML11_INLINE std::ostream& gray(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[30m";}
    return os;
}
TOML11_INLINE std::ostream& red(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[31m";}
    return os;
}
TOML11_INLINE std::ostream& green(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[32m";}
    return os;
}
TOML11_INLINE std::ostream& yellow(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[33m";}
    return os;
}
TOML11_INLINE std::ostream& blue(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[34m";}
    return os;
}
TOML11_INLINE std::ostream& magenta(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[35m";}
    return os;
}
TOML11_INLINE std::ostream& cyan   (std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[36m";}
    return os;
}
TOML11_INLINE std::ostream& white  (std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[37m";}
    return os;
}

} // ansi
} // color
} // toml
#endif // TOML11_COLOR_IMPL_HPP
