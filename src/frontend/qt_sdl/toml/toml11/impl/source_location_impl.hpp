#ifndef TOML11_SOURCE_LOCATION_IMPL_HPP
#define TOML11_SOURCE_LOCATION_IMPL_HPP

#include "../fwd/source_location_fwd.hpp"

#include "../color.hpp"
#include "../utility.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <cctype>

namespace toml
{

TOML11_INLINE source_location::source_location(const detail::region& r)
    : is_ok_(false),
      first_line_(1),
      first_column_(1),
      last_line_(1),
      last_column_(1),
      length_(0),
      file_name_("unknown file")
{
    if(r.is_ok())
    {
        this->is_ok_        = true;
        this->file_name_    = r.source_name();
        this->first_line_   = r.first_line_number();
        this->first_column_ = r.first_column_number();
        this->last_line_    = r.last_line_number();
        this->last_column_  = r.last_column_number();
        this->length_       = r.length();
        this->line_str_     = r.as_lines();
    }
}

TOML11_INLINE std::string const& source_location::first_line() const
{
    if(this->line_str_.size() == 0)
    {
        throw std::out_of_range("toml::source_location::first_line: `lines` is empty");
    }
    return this->line_str_.front();
}
TOML11_INLINE std::string const& source_location::last_line() const
{
    if(this->line_str_.size() == 0)
    {
        throw std::out_of_range("toml::source_location::first_line: `lines` is empty");
    }
    return this->line_str_.back();
}

namespace detail
{

TOML11_INLINE std::size_t integer_width_base10(std::size_t i) noexcept
{
    std::size_t width = 0;
    while(i != 0)
    {
        i /= 10;
        width += 1;
    }
    return width;
}

TOML11_INLINE std::ostringstream&
format_filename(std::ostringstream& oss, const source_location& loc)
{
    // --> example.toml
    oss << color::bold << color::blue << " --> " << color::reset
        << color::bold << loc.file_name() << '\n' << color::reset;
    return oss;
}

TOML11_INLINE std::ostringstream& format_empty_line(std::ostringstream& oss,
        const std::size_t lnw)
{
    //    |
    oss << detail::make_string(lnw + 1, ' ')
        << color::bold << color::blue << " |\n"  << color::reset;
    return oss;
}

TOML11_INLINE std::ostringstream& format_line(std::ostringstream& oss,
    const std::size_t lnw, const std::size_t linenum, const std::string& line)
{
    // 10 | key = "value"
    oss << ' ' << color::bold << color::blue
        << std::setw(static_cast<int>(lnw))
        << std::right << linenum << " | "  << color::reset;
    for(const char c : line)
    {
        if(std::isgraph(c) || c == ' ')
        {
            oss << c;
        }
        else
        {
            oss << show_char(c);
        }
    }
    oss << '\n';
    return oss;
}
TOML11_INLINE std::ostringstream& format_underline(std::ostringstream& oss,
        const std::size_t lnw, const std::size_t col, const std::size_t len,
        const std::string& msg)
{
    //    |       ^^^^^^^-- this part
    oss << make_string(lnw + 1, ' ')
        << color::bold << color::blue << " | " << color::reset;

    oss << make_string(col-1 /*1-origin*/, ' ')
        << color::bold << color::red
        << make_string(len, '^') << "-- "
        << color::reset << msg << '\n';

    return oss;
}

TOML11_INLINE std::string format_location_impl(const std::size_t lnw,
    const std::string& prev_fname,
    const source_location& loc, const std::string& msg)
{
    std::ostringstream oss;

    if(loc.file_name() != prev_fname)
    {
        format_filename(oss, loc);
        if( ! loc.lines().empty())
        {
            format_empty_line(oss, lnw);
        }
    }

    if(loc.lines().size() == 1)
    {
        // when column points LF, it exceeds the size of the first line.
        std::size_t underline_limit = 1;
        if(loc.first_line().size() < loc.first_column_number())
        {
            underline_limit = 1;
        }
        else
        {
            underline_limit = loc.first_line().size() - loc.first_column_number() + 1;
        }
        const auto underline_len = (std::min)(underline_limit, loc.length());

        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_number(), underline_len, msg);
    }
    else if(loc.lines().size() == 2)
    {
        const auto first_underline_len =
            loc.first_line().size() - loc.first_column_number() + 1;
        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_number(),
                first_underline_len, "");

        format_line(oss, lnw, loc.last_line_number(), loc.last_line());
        format_underline(oss, lnw, 1, loc.last_column_number(), msg);
    }
    else if(loc.lines().size() > 2)
    {
        const auto first_underline_len =
            loc.first_line().size() - loc.first_column_number() + 1;
        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_number(),
                first_underline_len, "and");

        if(loc.lines().size() == 3)
        {
            format_line(oss, lnw, loc.first_line_number()+1, loc.lines().at(1));
            format_underline(oss, lnw, 1, loc.lines().at(1).size(), "and");
        }
        else
        {
            format_line(oss, lnw, loc.first_line_number()+1, " ...");
            format_empty_line(oss, lnw);
        }
        format_line(oss, lnw, loc.last_line_number(), loc.last_line());
        format_underline(oss, lnw, 1, loc.last_column_number(), msg);
    }
    // if loc is empty, do nothing.
    return oss.str();
}

} // namespace detail
} // toml
#endif // TOML11_SOURCE_LOCATION_IMPL_HPP
