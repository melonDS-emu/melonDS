#ifndef TOML11_ERROR_INFO_FWD_HPP
#define TOML11_ERROR_INFO_FWD_HPP

#include "../source_location.hpp"
#include "../utility.hpp"

namespace toml
{

// error info returned from parser.
struct error_info
{
    error_info(std::string t, source_location l, std::string m, std::string s = "")
        : title_(std::move(t)), locations_{std::make_pair(std::move(l), std::move(m))},
          suffix_(std::move(s))
    {}

    error_info(std::string t, std::vector<std::pair<source_location, std::string>> l,
               std::string s = "")
        : title_(std::move(t)), locations_(std::move(l)), suffix_(std::move(s))
    {}

    std::string const& title() const noexcept {return title_;}
    std::string &      title()       noexcept {return title_;}

    std::vector<std::pair<source_location, std::string>> const&
    locations() const noexcept {return locations_;}

    void add_locations(source_location loc, std::string msg) noexcept
    {
        locations_.emplace_back(std::move(loc), std::move(msg));
    }

    std::string const& suffix() const noexcept {return suffix_;}
    std::string &      suffix()       noexcept {return suffix_;}

  private:

    std::string title_;
    std::vector<std::pair<source_location, std::string>> locations_;
    std::string suffix_; // hint or something like that
};

// forward decl
template<typename TypeConfig>
class basic_value;

namespace detail
{
inline error_info make_error_info_rec(error_info e)
{
    return e;
}
inline error_info make_error_info_rec(error_info e, std::string s)
{
    e.suffix() = s;
    return e;
}

template<typename TC, typename ... Ts>
error_info make_error_info_rec(error_info e,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail);

template<typename ... Ts>
error_info make_error_info_rec(error_info e,
        source_location loc, std::string msg, Ts&& ... tail)
{
    e.add_locations(std::move(loc), std::move(msg));
    return make_error_info_rec(std::move(e), std::forward<Ts>(tail)...);
}

} // detail

template<typename ... Ts>
error_info make_error_info(
    std::string title, source_location loc, std::string msg, Ts&& ... tail)
{
    error_info ei(std::move(title), std::move(loc), std::move(msg));
    return detail::make_error_info_rec(ei, std::forward<Ts>(tail) ... );
}

std::string format_error(const std::string& errkind, const error_info& err);
std::string format_error(const error_info& err);

// for custom error message
template<typename ... Ts>
std::string format_error(std::string title,
        source_location loc, std::string msg, Ts&& ... tail)
{
    return format_error("", make_error_info(std::move(title),
                std::move(loc), std::move(msg), std::forward<Ts>(tail)...));
}

std::ostream& operator<<(std::ostream& os, const error_info& e);

} // toml
#endif // TOML11_ERROR_INFO_FWD_HPP
