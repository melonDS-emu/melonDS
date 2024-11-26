#ifndef TOML11_ERROR_INFO_IMPL_HPP
#define TOML11_ERROR_INFO_IMPL_HPP

#include "../fwd/error_info_fwd.hpp"
#include "../fwd/color_fwd.hpp"

#include <sstream>

namespace toml
{

TOML11_INLINE std::string format_error(const std::string& errkind, const error_info& err)
{
    std::string errmsg;
    if( ! errkind.empty())
    {
        errmsg = errkind;
        errmsg += ' ';
    }
    errmsg += err.title();
    errmsg += '\n';

    const auto lnw = [&err]() {
        std::size_t width = 0;
        for(const auto& l : err.locations())
        {
            width = (std::max)(detail::integer_width_base10(l.first.last_line_number()), width);
        }
        return width;
    }();

    bool first = true;
    std::string prev_fname;
    for(const auto& lm : err.locations())
    {
        if( ! first)
        {
            std::ostringstream oss;
            oss << detail::make_string(lnw + 1, ' ')
                << color::bold << color::blue << " |" << color::reset
                << color::bold << " ...\n" << color::reset;
            oss << detail::make_string(lnw + 1, ' ')
                << color::bold << color::blue << " |\n" << color::reset;
            errmsg += oss.str();
        }

        const auto& l = lm.first;
        const auto& m = lm.second;

        errmsg += detail::format_location_impl(lnw, prev_fname, l, m);

        prev_fname = l.file_name();
        first = false;
    }

    errmsg += err.suffix();

    return errmsg;
}

TOML11_INLINE std::string format_error(const error_info& err)
{
    std::ostringstream oss;
    oss << color::red << color::bold << "[error]" << color::reset;
    return format_error(oss.str(), err);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const error_info& e)
{
    os << format_error(e);
    return os;
}

} // toml
#endif // TOML11_ERROR_INFO_IMPL_HPP
