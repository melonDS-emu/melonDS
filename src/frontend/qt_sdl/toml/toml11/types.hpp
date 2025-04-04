#ifndef TOML11_TYPES_HPP
#define TOML11_TYPES_HPP

#include "comments.hpp"
#include "error_info.hpp"
#include "format.hpp"
#include "ordered_map.hpp"
#include "value.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <cstdint>
#include <cstdio>

namespace toml
{

// forward decl
template<typename TypeConfig>
class basic_value;

// when you use a special integer type as toml::value::integer_type, parse must
// be able to read it. So, type_config has static member functions that read the
// integer_type as {dec, hex, oct, bin}-integer. But, in most cases, operator<<
// is enough. To make config easy, we provide the default read functions.
//
// Before this functions is called, syntax is checked and prefix(`0x` etc) and
// spacer(`_`) are removed.

template<typename T>
result<T, error_info>
read_dec_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_dec_integer: "
            "too large integer: current max digits = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_hex_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> std::hex >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_hex_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_oct_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> std::oct >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_oct_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_bin_int(const std::string& str, const source_location src)
{
    constexpr auto is_bounded =  std::numeric_limits<T>::is_bounded;
    constexpr auto max_digits =  std::numeric_limits<T>::digits;
    const auto max_value  = (std::numeric_limits<T>::max)();

    T val{0};
    T base{1};
    for(auto i = str.rbegin(); i != str.rend(); ++i)
    {
        const auto c = *i;
        if(c == '1')
        {
            val += base;
            // prevent `base` from overflow
            if(is_bounded && max_value / 2 < base && std::next(i) != str.rend())
            {
                base = 0;
            }
            else
            {
                base *= 2;
            }
        }
        else
        {
            assert(c == '0');

            if(is_bounded && max_value / 2 < base && std::next(i) != str.rend())
            {
                base = 0;
            }
            else
            {
                base *= 2;
            }
        }
    }
    if(base == 0)
    {
        return err(make_error_info("toml::parse_bin_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_int(const std::string& str, const source_location src, const std::uint8_t base)
{
    assert(base == 10 || base == 16 || base == 8 || base == 2);
    switch(base)
    {
        case  2: { return read_bin_int<T>(str, src); }
        case  8: { return read_oct_int<T>(str, src); }
        case 16: { return read_hex_int<T>(str, src); }
        default:
        {
            assert(base == 10);
            return read_dec_int<T>(str, src);
        }
    }
}

inline result<float, error_info>
read_hex_float(const std::string& str, const source_location src, float val)
{
#if defined(_MSC_VER) && ! defined(__clang__)
    const auto res = ::sscanf_s(str.c_str(), "%a", std::addressof(val));
#else
    const auto res = std::sscanf(str.c_str(), "%a", std::addressof(val));
#endif
    if(res != 1)
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read hexadecimal floating point value ",
            std::move(src), "here"));
    }
    return ok(val);
}
inline result<double, error_info>
read_hex_float(const std::string& str, const source_location src, double val)
{
#if defined(_MSC_VER) && ! defined(__clang__)
    const auto res = ::sscanf_s(str.c_str(), "%la", std::addressof(val));
#else
    const auto res = std::sscanf(str.c_str(), "%la", std::addressof(val));
#endif
    if(res != 1)
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read hexadecimal floating point value ",
            std::move(src), "here"));
    }
    return ok(val);
}
template<typename T>
cxx::enable_if_t<cxx::conjunction<
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, double>>,
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, float>>
    >::value, result<T, error_info>>
read_hex_float(const std::string&, const source_location src, T)
{
    return err(make_error_info("toml::parse_floating: failed to read "
        "floating point value because of unknown type in type_config",
        std::move(src), "here"));
}

template<typename T>
result<T, error_info>
read_dec_float(const std::string& str, const source_location src)
{
    T val;
    std::istringstream iss(str);
    iss >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read floating point value from stream",
            std::move(src), "here"));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_float(const std::string& str, const source_location src, const bool is_hex)
{
    if(is_hex)
    {
        return read_hex_float(str, src, T{});
    }
    else
    {
        return read_dec_float<T>(str, src);
    }
}

struct type_config
{
    using comment_type  = preserve_comments;

    using boolean_type  = bool;
    using integer_type  = std::int64_t;
    using floating_type = double;
    using string_type   = std::string;

    template<typename T>
    using array_type = std::vector<T>;
    template<typename K, typename T>
    using table_type = std::unordered_map<K, T>;

    static result<integer_type, error_info>
    parse_int(const std::string& str, const source_location src, const std::uint8_t base)
    {
        return read_int<integer_type>(str, src, base);
    }
    static result<floating_type, error_info>
    parse_float(const std::string& str, const source_location src, const bool is_hex)
    {
        return read_float<floating_type>(str, src, is_hex);
    }
};

using value = basic_value<type_config>;
using table = typename value::table_type;
using array = typename value::array_type;

struct ordered_type_config
{
    using comment_type  = preserve_comments;

    using boolean_type  = bool;
    using integer_type  = std::int64_t;
    using floating_type = double;
    using string_type   = std::string;

    template<typename T>
    using array_type = std::vector<T>;
    template<typename K, typename T>
    using table_type = ordered_map<K, T>;

    static result<integer_type, error_info>
    parse_int(const std::string& str, const source_location src, const std::uint8_t base)
    {
        return read_int<integer_type>(str, src, base);
    }
    static result<floating_type, error_info>
    parse_float(const std::string& str, const source_location src, const bool is_hex)
    {
        return read_float<floating_type>(str, src, is_hex);
    }
};

using ordered_value = basic_value<ordered_type_config>;
using ordered_table = typename ordered_value::table_type;
using ordered_array = typename ordered_value::array_type;

// ----------------------------------------------------------------------------
// meta functions for internal use

namespace detail
{

// ----------------------------------------------------------------------------
// check if type T has all the needed member types

struct has_comment_type_impl
{
    template<typename T> static std::true_type  check(typename T::comment_type*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_comment_type = decltype(has_comment_type_impl::check<T>(nullptr));

struct has_integer_type_impl
{
    template<typename T> static std::true_type  check(typename T::integer_type*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_integer_type = decltype(has_integer_type_impl::check<T>(nullptr));

struct has_floating_type_impl
{
    template<typename T> static std::true_type  check(typename T::floating_type*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_floating_type = decltype(has_floating_type_impl::check<T>(nullptr));

struct has_string_type_impl
{
    template<typename T> static std::true_type  check(typename T::string_type*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_string_type = decltype(has_string_type_impl::check<T>(nullptr));

struct has_array_type_impl
{
    template<typename T> static std::true_type  check(typename T::template array_type<int>*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_array_type = decltype(has_array_type_impl::check<T>(nullptr));

struct has_table_type_impl
{
    template<typename T> static std::true_type  check(typename T::template table_type<int, int>*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_table_type = decltype(has_table_type_impl::check<T>(nullptr));

struct has_parse_int_impl
{
    template<typename T> static std::true_type  check(decltype(std::declval<T>().parse_int(
            std::declval<const std::string&>(),
            std::declval<const source_location>(),
            std::declval<const std::uint8_t>()
        ))*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_parse_int = decltype(has_parse_int_impl::check<T>(nullptr));

struct has_parse_float_impl
{
    template<typename T> static std::true_type  check(decltype(std::declval<T>().parse_float(
            std::declval<const std::string&>(),
            std::declval<const source_location>(),
            std::declval<const bool>()
        ))*);
    template<typename T> static std::false_type check(...);
};
template<typename T>
using has_parse_float = decltype(has_parse_float_impl::check<T>(nullptr));

template<typename T>
using is_type_config = cxx::conjunction<
    has_comment_type<T>,
    has_integer_type<T>,
    has_floating_type<T>,
    has_string_type<T>,
    has_array_type<T>,
    has_table_type<T>,
    has_parse_int<T>,
    has_parse_float<T>
    >;

} // namespace detail
} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
extern template class basic_value<type_config>;
extern template class basic_value<ordered_type_config>;
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_TYPES_HPP
