#ifndef TOML11_VALUE_T_FWD_HPP
#define TOML11_VALUE_T_FWD_HPP

#include "../compat.hpp"
#include "../format.hpp"

#include <iosfwd>
#include <string>
#include <type_traits>

#include <cstdint>

namespace toml
{

// forward decl
template<typename TypeConfig>
class basic_value;

// ----------------------------------------------------------------------------
// enum representing toml types

enum class value_t : std::uint8_t
{
    empty           =  0,
    boolean         =  1,
    integer         =  2,
    floating        =  3,
    string          =  4,
    offset_datetime =  5,
    local_datetime  =  6,
    local_date      =  7,
    local_time      =  8,
    array           =  9,
    table           = 10
};

std::ostream& operator<<(std::ostream& os, value_t t);
std::string to_string(value_t t);


// ----------------------------------------------------------------------------
// meta functions for internal use

namespace detail
{

template<value_t V>
using value_t_constant = std::integral_constant<value_t, V>;

template<typename T, typename Value>
struct type_to_enum : value_t_constant<value_t::empty> {};

template<typename V> struct type_to_enum<typename V::boolean_type        , V> : value_t_constant<value_t::boolean        > {};
template<typename V> struct type_to_enum<typename V::integer_type        , V> : value_t_constant<value_t::integer        > {};
template<typename V> struct type_to_enum<typename V::floating_type       , V> : value_t_constant<value_t::floating       > {};
template<typename V> struct type_to_enum<typename V::string_type         , V> : value_t_constant<value_t::string         > {};
template<typename V> struct type_to_enum<typename V::offset_datetime_type, V> : value_t_constant<value_t::offset_datetime> {};
template<typename V> struct type_to_enum<typename V::local_datetime_type , V> : value_t_constant<value_t::local_datetime > {};
template<typename V> struct type_to_enum<typename V::local_date_type     , V> : value_t_constant<value_t::local_date     > {};
template<typename V> struct type_to_enum<typename V::local_time_type     , V> : value_t_constant<value_t::local_time     > {};
template<typename V> struct type_to_enum<typename V::array_type          , V> : value_t_constant<value_t::array          > {};
template<typename V> struct type_to_enum<typename V::table_type          , V> : value_t_constant<value_t::table          > {};

template<value_t V, typename Value>
struct enum_to_type { using type = void; };

template<typename V> struct enum_to_type<value_t::boolean        , V> { using type = typename V::boolean_type        ; };
template<typename V> struct enum_to_type<value_t::integer        , V> { using type = typename V::integer_type        ; };
template<typename V> struct enum_to_type<value_t::floating       , V> { using type = typename V::floating_type       ; };
template<typename V> struct enum_to_type<value_t::string         , V> { using type = typename V::string_type         ; };
template<typename V> struct enum_to_type<value_t::offset_datetime, V> { using type = typename V::offset_datetime_type; };
template<typename V> struct enum_to_type<value_t::local_datetime , V> { using type = typename V::local_datetime_type ; };
template<typename V> struct enum_to_type<value_t::local_date     , V> { using type = typename V::local_date_type     ; };
template<typename V> struct enum_to_type<value_t::local_time     , V> { using type = typename V::local_time_type     ; };
template<typename V> struct enum_to_type<value_t::array          , V> { using type = typename V::array_type          ; };
template<typename V> struct enum_to_type<value_t::table          , V> { using type = typename V::table_type          ; };

template<value_t V, typename Value>
using enum_to_type_t = typename enum_to_type<V, Value>::type;

template<value_t V>
struct enum_to_fmt_type { using type = void; };

template<> struct enum_to_fmt_type<value_t::boolean        > { using type = boolean_format_info        ; };
template<> struct enum_to_fmt_type<value_t::integer        > { using type = integer_format_info        ; };
template<> struct enum_to_fmt_type<value_t::floating       > { using type = floating_format_info       ; };
template<> struct enum_to_fmt_type<value_t::string         > { using type = string_format_info         ; };
template<> struct enum_to_fmt_type<value_t::offset_datetime> { using type = offset_datetime_format_info; };
template<> struct enum_to_fmt_type<value_t::local_datetime > { using type = local_datetime_format_info ; };
template<> struct enum_to_fmt_type<value_t::local_date     > { using type = local_date_format_info     ; };
template<> struct enum_to_fmt_type<value_t::local_time     > { using type = local_time_format_info     ; };
template<> struct enum_to_fmt_type<value_t::array          > { using type = array_format_info          ; };
template<> struct enum_to_fmt_type<value_t::table          > { using type = table_format_info          ; };

template<value_t V>
using enum_to_fmt_type_t = typename enum_to_fmt_type<V>::type;

template<typename T, typename Value>
struct is_exact_toml_type0 : cxx::disjunction<
    std::is_same<T, typename Value::boolean_type        >,
    std::is_same<T, typename Value::integer_type        >,
    std::is_same<T, typename Value::floating_type       >,
    std::is_same<T, typename Value::string_type         >,
    std::is_same<T, typename Value::offset_datetime_type>,
    std::is_same<T, typename Value::local_datetime_type >,
    std::is_same<T, typename Value::local_date_type     >,
    std::is_same<T, typename Value::local_time_type     >,
    std::is_same<T, typename Value::array_type          >,
    std::is_same<T, typename Value::table_type          >
    >{};
template<typename T, typename V> struct is_exact_toml_type: is_exact_toml_type0<cxx::remove_cvref_t<T>, V> {};
template<typename T, typename V> struct is_not_toml_type : cxx::negation<is_exact_toml_type<T, V>> {};

} // namespace detail
} // namespace toml
#endif // TOML11_VALUE_T_FWD_HPP
