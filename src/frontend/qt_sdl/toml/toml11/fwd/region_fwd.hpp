#ifndef TOML11_REGION_FWD_HPP
#define TOML11_REGION_FWD_HPP

#include "../location.hpp"

#include <string>
#include <vector>

#include <cassert>

namespace toml
{
namespace detail
{

//
// To represent where is a toml::value defined, or where does an error occur.
// Stored in toml::value. source_location will be constructed based on this.
//
class region
{
  public:

    using char_type       = location::char_type;
    using container_type  = location::container_type;
    using difference_type = location::difference_type;
    using source_ptr      = location::source_ptr;

    using iterator       = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

  public:

    // a value that is constructed manually does not have input stream info
    region()
        : source_(nullptr), source_name_(""), length_(0),
          first_line_(0), first_column_(0), last_line_(0), last_column_(0)
    {}

    // a value defined in [first, last).
    // Those source must be the same. Instread, `region` does not make sense.
    region(const location& first, const location& last);

    // shorthand of [loc, loc+1)
    explicit region(const location& loc);

    ~region() = default;
    region(const region&) = default;
    region(region&&)      = default;
    region& operator=(const region&) = default;
    region& operator=(region&&)      = default;

    bool is_ok() const noexcept { return static_cast<bool>(this->source_); }

    operator bool() const noexcept { return this->is_ok(); }

    std::size_t length() const noexcept {return this->length_;}

    std::size_t first_line_number() const noexcept
    {
        return this->first_line_;
    }
    std::size_t first_column_number() const noexcept
    {
        return this->first_column_;
    }
    std::size_t last_line_number() const noexcept
    {
        return this->last_line_;
    }
    std::size_t last_column_number() const noexcept
    {
        return this->last_column_;
    }

    char_type at(std::size_t i) const;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    std::string as_string() const;
    std::vector<std::string> as_lines() const;

    source_ptr const&  source()      const noexcept {return this->source_;}
    std::string const& source_name() const noexcept {return this->source_name_;}

  private:

    source_ptr  source_;
    std::string source_name_;
    std::size_t length_;
    std::size_t first_;
    std::size_t first_line_;
    std::size_t first_column_;
    std::size_t last_;
    std::size_t last_line_;
    std::size_t last_column_;
};

} // namespace detail
} // namespace toml
#endif // TOML11_REGION_FWD_HPP
