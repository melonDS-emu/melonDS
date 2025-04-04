#ifndef TOML11_SOURCE_LOCATION_FWD_HPP
#define TOML11_SOURCE_LOCATION_FWD_HPP

#include "../region.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace toml
{

// A struct to contain location in a toml file.
struct source_location
{
  public:

    explicit source_location(const detail::region& r);
    ~source_location() = default;
    source_location(source_location const&) = default;
    source_location(source_location &&)     = default;
    source_location& operator=(source_location const&) = default;
    source_location& operator=(source_location &&)     = default;

    bool is_ok() const noexcept {return this->is_ok_;}
    std::size_t length() const noexcept {return this->length_;}

    std::size_t first_line_number()   const noexcept {return this->first_line_;}
    std::size_t first_column_number() const noexcept {return this->first_column_;}
    std::size_t last_line_number()    const noexcept {return this->last_line_;}
    std::size_t last_column_number()  const noexcept {return this->last_column_;}

    std::string const& file_name()  const noexcept {return this->file_name_;}

    std::size_t num_lines() const noexcept {return this->line_str_.size();}

    std::string const& first_line() const;
    std::string const& last_line() const;

    std::vector<std::string> const& lines() const noexcept {return line_str_;}

  private:

    bool        is_ok_;
    std::size_t first_line_;
    std::size_t first_column_;
    std::size_t last_line_;
    std::size_t last_column_;
    std::size_t length_;
    std::string file_name_;
    std::vector<std::string> line_str_;
};

namespace detail
{

std::size_t integer_width_base10(std::size_t i) noexcept;

inline std::size_t line_width() noexcept {return 0;}

template<typename ... Ts>
std::size_t line_width(const source_location& loc, const std::string& /*msg*/,
        const Ts& ... tail) noexcept
{
    return (std::max)(
            integer_width_base10(loc.last_line_number()), line_width(tail...));
}

std::ostringstream&
format_filename(std::ostringstream& oss, const source_location& loc);

std::ostringstream&
format_empty_line(std::ostringstream& oss, const std::size_t lnw);

std::ostringstream& format_line(std::ostringstream& oss,
    const std::size_t lnw, const std::size_t linenum, const std::string& line);

std::ostringstream& format_underline(std::ostringstream& oss,
        const std::size_t lnw, const std::size_t col, const std::size_t len,
        const std::string& msg);

std::string format_location_impl(const std::size_t lnw,
    const std::string& prev_fname,
    const source_location& loc, const std::string& msg);

inline std::string format_location_rec(const std::size_t, const std::string&)
{
    return "";
}

template<typename ... Ts>
std::string format_location_rec(const std::size_t lnw,
        const std::string& prev_fname,
        const source_location& loc, const std::string& msg,
        const Ts& ... tail)
{
    return format_location_impl(lnw, prev_fname, loc, msg) +
           format_location_rec(lnw, loc.file_name(), tail...);
}

} // namespace detail

// format a location info without title
template<typename ... Ts>
std::string format_location(
        const source_location& loc, const std::string& msg, const Ts& ... tail)
{
    const auto lnw = detail::line_width(loc, msg, tail...);

    const std::string f(""); // at the 1st iteration, no prev_filename is given
    return detail::format_location_rec(lnw, f, loc, msg, tail...);
}

} // toml
#endif // TOML11_SOURCE_LOCATION_FWD_HPP
