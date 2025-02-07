#ifndef TOML11_LOCATION_IMPL_HPP
#define TOML11_LOCATION_IMPL_HPP

#include "../fwd/location_fwd.hpp"
#include "../utility.hpp"
#include "../version.hpp"

namespace toml
{
namespace detail
{

TOML11_INLINE void location::advance(std::size_t n) noexcept
{
    assert(this->is_ok());
    if(this->location_ + n < this->source_->size())
    {
        this->advance_line_number(n);
        this->location_ += n;
    }
    else
    {
        this->advance_line_number(this->source_->size() - this->location_);
        this->location_ = this->source_->size();
    }
}
TOML11_INLINE void location::retrace(std::size_t n) noexcept
{
    assert(this->is_ok());
    if(this->location_ < n)
    {
        this->location_ = 0;
        this->line_number_ = 1;
    }
    else
    {
        this->retrace_line_number(n);
        this->location_ -= n;
    }
}

TOML11_INLINE bool location::eof() const noexcept
{
    assert(this->is_ok());
    return this->location_ >= this->source_->size();
}
TOML11_INLINE location::char_type location::current() const
{
    assert(this->is_ok());
    if(this->eof()) {return '\0';}

    assert(this->location_ < this->source_->size());
    return this->source_->at(this->location_);
}

TOML11_INLINE location::char_type location::peek()
{
    assert(this->is_ok());
    if(this->location_ >= this->source_->size())
    {
        return '\0';
    }
    else
    {
        return this->source_->at(this->location_ + 1);
    }
}

TOML11_INLINE void location::set_location(const std::size_t loc) noexcept
{
    if(this->location_ == loc)
    {
        return ;
    }

    if(loc == 0)
    {
        this->line_number_ = 1;
    }
    else if(this->location_ < loc)
    {
        const auto d = loc - this->location_;
        this->advance_line_number(d);
    }
    else
    {
        const auto d = this->location_ - loc;
        this->retrace_line_number(d);
    }
    this->location_ = loc;
}

TOML11_INLINE std::string location::get_line() const
{
    assert(this->is_ok());
    const auto iter = std::next(this->source_->cbegin(), static_cast<difference_type>(this->location_));
    const auto riter = cxx::make_reverse_iterator(iter);

    const auto prev = std::find(riter, this->source_->crend(), char_type('\n'));
    const auto next = std::find(iter,  this->source_->cend(),  char_type('\n'));

    return make_string(std::next(prev.base()), next);
}
TOML11_INLINE std::size_t location::column_number() const noexcept
{
    assert(this->is_ok());
    const auto iter  = std::next(this->source_->cbegin(), static_cast<difference_type>(this->location_));
    const auto riter = cxx::make_reverse_iterator(iter);
    const auto prev  = std::find(riter, this->source_->crend(), char_type('\n'));

    assert(prev.base() <= iter);
    return static_cast<std::size_t>(std::distance(prev.base(), iter) + 1); // 1-origin
}


TOML11_INLINE void location::advance_line_number(const std::size_t n)
{
    assert(this->is_ok());
    assert(this->location_ + n <= this->source_->size());

    const auto iter = this->source_->cbegin();
    this->line_number_ += static_cast<std::size_t>(std::count(
        std::next(iter, static_cast<difference_type>(this->location_)),
        std::next(iter, static_cast<difference_type>(this->location_ + n)),
        char_type('\n')));

    return;
}
TOML11_INLINE void location::retrace_line_number(const std::size_t n)
{
    assert(this->is_ok());
    assert(n <= this->location_); // loc - n >= 0

    const auto iter = this->source_->cbegin();
    const auto dline_num = static_cast<std::size_t>(std::count(
        std::next(iter, static_cast<difference_type>(this->location_ - n)),
        std::next(iter, static_cast<difference_type>(this->location_)),
        char_type('\n')));

    if(this->line_number_ <= dline_num)
    {
        this->line_number_ = 1;
    }
    else
    {
        this->line_number_ -= dline_num;
    }
    return;
}

TOML11_INLINE bool operator==(const location& lhs, const location& rhs) noexcept
{
    if( ! lhs.is_ok() || ! rhs.is_ok())
    {
        return (!lhs.is_ok()) && (!rhs.is_ok());
    }
    return lhs.source()       == rhs.source()      &&
           lhs.source_name()  == rhs.source_name() &&
           lhs.get_location() == rhs.get_location();
}
TOML11_INLINE bool operator!=(const location& lhs, const location& rhs)
{
    return !(lhs == rhs);
}

TOML11_INLINE location prev(const location& loc)
{
    location p(loc);
    p.retrace(1);
    return p;
}
TOML11_INLINE location next(const location& loc)
{
    location p(loc);
    p.advance(1);
    return p;
}

TOML11_INLINE location make_temporary_location(const std::string& str) noexcept
{
    location::container_type cont(str.size());
    std::transform(str.begin(), str.end(), cont.begin(),
        [](const std::string::value_type& c) {
            return cxx::bit_cast<location::char_type>(c);
        });
    return location(std::make_shared<const location::container_type>(
            std::move(cont)), "internal temporary");
}

TOML11_INLINE result<location, none_t>
find(const location& first, const location& last, const location::char_type val)
{
    return find_if(first, last, [val](const location::char_type c) {
            return c == val;
        });
}
TOML11_INLINE result<location, none_t>
rfind(const location& first, const location& last, const location::char_type val)
{
    return rfind_if(first, last, [val](const location::char_type c) {
            return c == val;
        });
}

TOML11_INLINE std::size_t
count(const location& first, const location& last, const location::char_type& c)
{
    if(first.source() != last.source())             { return 0; }
    if(first.get_location() >= last.get_location()) { return 0; }

    auto loc = first;
    std::size_t num = 0;
    while(loc.get_location() != last.get_location())
    {
        if(loc.current() == c)
        {
            num += 1;
        }
        loc.advance();
    }
    return num;
}

} // detail
} // toml
#endif // TOML11_LOCATION_HPP
