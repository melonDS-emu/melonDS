#ifndef TOML11_LOCATION_FWD_HPP
#define TOML11_LOCATION_FWD_HPP

#include "../result.hpp"

#include <memory>
#include <string>
#include <vector>

namespace toml
{
namespace detail
{

class region; // fwd decl

//
// To represent where we are reading in the parse functions.
// Since it "points" somewhere in the input stream, the length is always 1.
//
class location
{
  public:

    using char_type       = unsigned char; // must be unsigned
    using container_type  = std::vector<char_type>;
    using difference_type = typename container_type::difference_type; // to suppress sign-conversion warning
    using source_ptr      = std::shared_ptr<const container_type>;

  public:

    location(source_ptr src, std::string src_name)
        : source_(std::move(src)), source_name_(std::move(src_name)),
          location_(0), line_number_(1)
    {}

    location(const location&) = default;
    location(location&&)      = default;
    location& operator=(const location&) = default;
    location& operator=(location&&)      = default;
    ~location() = default;

    void advance(std::size_t n = 1) noexcept;
    void retrace(std::size_t n = 1) noexcept;

    bool is_ok() const noexcept { return static_cast<bool>(this->source_); }

    bool eof() const noexcept;
    char_type current() const;

    char_type peek();

    std::size_t get_location() const noexcept
    {
        return this->location_;
    }
    void set_location(const std::size_t loc) noexcept;

    std::size_t line_number() const noexcept
    {
        return this->line_number_;
    }
    std::string get_line() const;
    std::size_t column_number() const noexcept;

    source_ptr const&  source()      const noexcept {return this->source_;}
    std::string const& source_name() const noexcept {return this->source_name_;}

  private:

    void advance_line_number(const std::size_t n);
    void retrace_line_number(const std::size_t n);

  private:

    friend region;

  private:

    source_ptr  source_;
    std::string source_name_;
    std::size_t location_; // std::vector<>::difference_type is signed
    std::size_t line_number_;
};

bool operator==(const location& lhs, const location& rhs) noexcept;
bool operator!=(const location& lhs, const location& rhs);

location prev(const location& loc);
location next(const location& loc);
location make_temporary_location(const std::string& str) noexcept;

template<typename F>
result<location, none_t>
find_if(const location& first, const location& last, const F& func) noexcept
{
    if(first.source() != last.source())             { return err(); }
    if(first.get_location() >= last.get_location()) { return err(); }

    auto loc = first;
    while(loc.get_location() != last.get_location())
    {
        if(func(loc.current()))
        {
            return ok(loc);
        }
        loc.advance();
    }
    return err();
}

template<typename F>
result<location, none_t>
rfind_if(location first, const location& last, const F& func)
{
    if(first.source() != last.source())             { return err(); }
    if(first.get_location() >= last.get_location()) { return err(); }

    auto loc = last;
    while(loc.get_location() != first.get_location())
    {
        if(func(loc.current()))
        {
            return ok(loc);
        }
        loc.retrace();
    }
    if(func(first.current()))
    {
        return ok(first);
    }
    return err();
}

result<location, none_t> find(const location& first, const location& last,
                              const location::char_type val);
result<location, none_t> rfind(const location& first, const location& last,
                               const location::char_type val);

std::size_t count(const location& first, const location& last,
                  const location::char_type& c);

} // detail
} // toml
#endif // TOML11_LOCATION_FWD_HPP
