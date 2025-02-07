#ifndef TOML11_SCANNER_IMPL_HPP
#define TOML11_SCANNER_IMPL_HPP

#include "../fwd/scanner_fwd.hpp"
#include "../utility.hpp"

namespace toml
{
namespace detail
{

TOML11_INLINE scanner_storage::scanner_storage(const scanner_storage& other)
    : scanner_(nullptr)
{
    if(other.is_ok())
    {
        scanner_.reset(other.get().clone());
    }
}
TOML11_INLINE scanner_storage& scanner_storage::operator=(const scanner_storage& other)
{
    if(this == std::addressof(other)) {return *this;}
    if(other.is_ok())
    {
        scanner_.reset(other.get().clone());
    }
    return *this;
}

TOML11_INLINE region scanner_storage::scan(location& loc) const
{
    assert(this->is_ok());
    return this->scanner_->scan(loc);
}

TOML11_INLINE std::string scanner_storage::expected_chars(location& loc) const
{
    assert(this->is_ok());
    return this->scanner_->expected_chars(loc);
}

TOML11_INLINE scanner_base& scanner_storage::get() const noexcept
{
    assert(this->is_ok());
    return *scanner_;
}

TOML11_INLINE std::string scanner_storage::name() const
{
    assert(this->is_ok());
    return this->scanner_->name();
}

// ----------------------------------------------------------------------------

TOML11_INLINE region character::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    if(loc.current() == this->value_)
    {
        const auto first = loc;
        loc.advance(1);
        return region(first, loc);
    }
    return region{};
}

TOML11_INLINE std::string character::expected_chars(location&) const
{
    return show_char(value_);
}

TOML11_INLINE scanner_base* character::clone() const
{
    return new character(*this);
}

TOML11_INLINE std::string character::name() const
{
    return "character{" + show_char(value_) + "}";
}

// ----------------------------------------------------------------------------

TOML11_INLINE region character_either::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    for(const auto c : this->chars_)
    {
        if(loc.current() == c)
        {
            const auto first = loc;
            loc.advance(1);
            return region(first, loc);
        }
    }
    return region{};
}

TOML11_INLINE std::string character_either::expected_chars(location&) const
{
    assert( ! chars_.empty());

    std::string expected;
    if(chars_.size() == 1)
    {
        expected += show_char(chars_.at(0));
    }
    else if(chars_.size() == 2)
    {
        expected += show_char(chars_.at(0)) + " or " + show_char(chars_.at(1));
    }
    else
    {
        for(std::size_t i=0; i<chars_.size(); ++i)
        {
            if(i != 0)
            {
                expected += ", ";
            }
            if(i + 1 == chars_.size())
            {
                expected += "or ";
            }
            expected += show_char(chars_.at(i));
        }
    }
    return expected;
}

TOML11_INLINE scanner_base* character_either::clone() const
{
    return new character_either(*this);
}

TOML11_INLINE void character_either::push_back(const char_type c)
{
    chars_.push_back(c);
}

TOML11_INLINE std::string character_either::name() const
{
    std::string n("character_either{");
    for(const auto c : this->chars_)
    {
        n += show_char(c);
        n += ", ";
    }
    if( ! this->chars_.empty())
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// character_in_range

TOML11_INLINE region character_in_range::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    const auto curr = loc.current();
    if(this->from_ <= curr && curr <= this->to_)
    {
        const auto first = loc;
        loc.advance(1);
        return region(first, loc);
    }
    return region{};
}

TOML11_INLINE std::string character_in_range::expected_chars(location&) const
{
    std::string expected("from `");
    expected += show_char(from_);
    expected += "` to `";
    expected += show_char(to_);
    expected += "`";
    return expected;
}

TOML11_INLINE scanner_base* character_in_range::clone() const
{
    return new character_in_range(*this);
}

TOML11_INLINE std::string character_in_range::name() const
{
    return "character_in_range{" + show_char(from_) + "," + show_char(to_) + "}";
}

// ----------------------------------------------------------------------------
// literal

TOML11_INLINE region literal::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<size_; ++i)
    {
        if(loc.eof() || char_type(value_[i]) != loc.current())
        {
            loc = first;
            return region{};
        }
        loc.advance(1);
    }
    return region(first, loc);
}

TOML11_INLINE std::string literal::expected_chars(location&) const
{
    return std::string(value_);
}

TOML11_INLINE scanner_base* literal::clone() const
{
    return new literal(*this);
}

TOML11_INLINE std::string literal::name() const
{
    return std::string("literal{") + std::string(value_, size_) + "}";
}

// ----------------------------------------------------------------------------
// sequence

TOML11_INLINE region sequence::scan(location& loc) const
{
    const auto first = loc;
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string sequence::expected_chars(location& loc) const
{
    const auto first = loc;
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if( ! reg.is_ok())
        {
            return other.expected_chars(loc);
        }
    }
    assert(false);
    return ""; // XXX
}

TOML11_INLINE scanner_base* sequence::clone() const
{
    return new sequence(*this);
}

TOML11_INLINE std::string sequence::name() const
{
    std::string n("sequence{");
    for(const auto& other : others_)
    {
        n += other.name();
        n += ", ";
    }
    if( ! this->others_.empty())
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// either

TOML11_INLINE region either::scan(location& loc) const
{
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if(reg.is_ok())
        {
            return reg;
        }
    }
    return region{};
}

TOML11_INLINE std::string either::expected_chars(location& loc) const
{
    assert( ! others_.empty());

    std::string expected = others_.at(0).expected_chars(loc);
    if(others_.size() == 2)
    {
        expected += " or ";
        expected += others_.at(1).expected_chars(loc);
    }
    else
    {
        for(std::size_t i=1; i<others_.size(); ++i)
        {
            expected += ", ";
            if(i + 1 == others_.size())
            {
                expected += "or ";
            }
            expected += others_.at(i).expected_chars(loc);
        }
    }
    return expected;
}

TOML11_INLINE scanner_base* either::clone() const
{
    return new either(*this);
}

TOML11_INLINE std::string either::name() const
{
    std::string n("either{");
    for(const auto& other : others_)
    {
        n += other.name();
        n += ", ";
    }
    if( ! this->others_.empty())
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// repeat_exact

TOML11_INLINE region repeat_exact::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string repeat_exact::expected_chars(location& loc) const
{
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            return other_.expected_chars(loc);
        }
    }
    assert(false);
    return "";
}

TOML11_INLINE scanner_base* repeat_exact::clone() const
{
    return new repeat_exact(*this);
}

TOML11_INLINE std::string repeat_exact::name() const
{
    return "repeat_exact{" + std::to_string(length_) + ", " + other_.name() + "}";
}

// ----------------------------------------------------------------------------
// repeat_at_least

TOML11_INLINE region repeat_at_least::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    while( ! loc.eof())
    {
        const auto checkpoint = loc;
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = checkpoint;
            return region(first, loc);
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string repeat_at_least::expected_chars(location& loc) const
{
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            return other_.expected_chars(loc);
        }
    }
    assert(false);
    return "";
}

TOML11_INLINE scanner_base* repeat_at_least::clone() const
{
    return new repeat_at_least(*this);
}

TOML11_INLINE std::string repeat_at_least::name() const
{
    return "repeat_at_least{" + std::to_string(length_) + ", " + other_.name() + "}";
}

// ----------------------------------------------------------------------------
// maybe

TOML11_INLINE region maybe::scan(location& loc) const
{
    const auto first = loc;
    const auto reg = other_.scan(loc);
    if( ! reg.is_ok())
    {
        loc = first;
    }
    return region(first, loc);
}

TOML11_INLINE std::string maybe::expected_chars(location&) const
{
    return "";
}

TOML11_INLINE scanner_base* maybe::clone() const
{
    return new maybe(*this);
}

TOML11_INLINE std::string maybe::name() const
{
    return "maybe{" + other_.name() + "}";
}

} // detail
} // toml
#endif // TOML11_SCANNER_IMPL_HPP
