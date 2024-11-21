#ifndef TOML11_GET_HPP
#define TOML11_GET_HPP

#include <algorithm>

#include "from.hpp"
#include "types.hpp"
#include "value.hpp"

#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif // string_view

namespace toml
{

// ============================================================================
// T is toml::value; identity transformation.

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T>&
get(basic_value<TC>& v)
{
    return v;
}

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T> const&
get(const basic_value<TC>& v)
{
    return v;
}

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T>
get(basic_value<TC>&& v)
{
    return basic_value<TC>(std::move(v));
}

// ============================================================================
// exact toml::* type

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T> &
get(basic_value<TC>& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(v);
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T> const&
get(const basic_value<TC>& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(v);
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T>
get(basic_value<TC>&& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(std::move(v));
}

// ============================================================================
// T is toml::basic_value<U>

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_basic_value<T>,
    cxx::negation<std::is_same<T, basic_value<TC>>>
    >::value, T>
get(basic_value<TC> v)
{
    return T(std::move(v));
}

// ============================================================================
// integer convertible from toml::value::integer_type

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_integral<T>,
    cxx::negation<std::is_same<T, bool>>,
    detail::is_not_toml_type<T, basic_value<TC>>,
    cxx::negation<detail::has_from_toml_method<T, TC>>,
    cxx::negation<detail::has_specialized_from<T>>
    >::value, T>
get(const basic_value<TC>& v)
{
    return static_cast<T>(v.as_integer());
}

// ============================================================================
// floating point convertible from toml::value::floating_type

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_floating_point<T>,
    detail::is_not_toml_type<T, basic_value<TC>>,
    cxx::negation<detail::has_from_toml_method<T, TC>>,
    cxx::negation<detail::has_specialized_from<T>>
    >::value, T>
get(const basic_value<TC>& v)
{
    return static_cast<T>(v.as_floating());
}

// ============================================================================
// std::string with different char/trait/allocator

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_not_toml_type<T, basic_value<TC>>,
    detail::is_1byte_std_basic_string<T>
    >::value, T>
get(const basic_value<TC>& v)
{
    return detail::string_conv<cxx::remove_cvref_t<T>>(v.as_string());
}

// ============================================================================
// std::string_view

#if defined(TOML11_HAS_STRING_VIEW)

template<typename T, typename TC>
cxx::enable_if_t<detail::is_string_view_of<T, typename basic_value<TC>::string_type>::value, T>
get(const basic_value<TC>& v)
{
    return T(v.as_string());
}

#endif // string_view

// ============================================================================
// std::chrono::duration from toml::local_time

template<typename T, typename TC>
cxx::enable_if_t<detail::is_chrono_duration<T>::value, T>
get(const basic_value<TC>& v)
{
    return std::chrono::duration_cast<T>(
            std::chrono::nanoseconds(v.as_local_time()));
}

// ============================================================================
// std::chrono::system_clock::time_point from toml::datetime variants

template<typename T, typename TC>
cxx::enable_if_t<
    std::is_same<std::chrono::system_clock::time_point, T>::value, T>
get(const basic_value<TC>& v)
{
    switch(v.type())
    {
        case value_t::local_date:
        {
            return std::chrono::system_clock::time_point(v.as_local_date());
        }
        case value_t::local_datetime:
        {
            return std::chrono::system_clock::time_point(v.as_local_datetime());
        }
        case value_t::offset_datetime:
        {
            return std::chrono::system_clock::time_point(v.as_offset_datetime());
        }
        default:
        {
            const auto loc = v.location();
            throw type_error(format_error("toml::get: "
                "bad_cast to std::chrono::system_clock::time_point", loc,
                "the actual type is " + to_string(v.type())), loc);
        }
    }
}

// ============================================================================
// forward declaration to use this recursively. ignore this and go ahead.

// array-like (w/ push_back)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_container<T>,                            // T is a container
    detail::has_push_back_method<T>,                    // .push_back() works
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::array
    cxx::negation<detail::is_std_basic_string<T>>,      // but not std::basic_string<CharT>
#if defined(TOML11_HAS_STRING_VIEW)
    cxx::negation<detail::is_std_basic_string_view<T>>,      // but not std::basic_string_view<CharT>
#endif
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>&);

// std::array
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_array<T>::value, T>
get(const basic_value<TC>&);

// std::forward_list
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_forward_list<T>::value, T>
get(const basic_value<TC>&);

// std::pair<T1, T2>
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_pair<T>::value, T>
get(const basic_value<TC>&);

// std::tuple<T1, T2, ...>
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_tuple<T>::value, T>
get(const basic_value<TC>&);

// std::map<key, value> (key is convertible from toml::value::key_type)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    std::is_convertible<typename basic_value<TC>::key_type,
                        typename T::key_type>,          // keys are convertible
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v);

// std::map<key, value> (key is not convertible from toml::value::key_type, but
// is a std::basic_string)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    cxx::negation<std::is_convertible<typename basic_value<TC>::key_type,
        typename T::key_type>>,                         // keys are NOT convertible
    detail::is_1byte_std_basic_string<typename T::key_type>, // is std::basic_string
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v);

// toml::from<T>::from_toml(v)
template<typename T, typename TC>
cxx::enable_if_t<detail::has_specialized_from<T>::value, T>
get(const basic_value<TC>&);

// has T.from_toml(v) but no from<T>
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::has_from_toml_method<T, TC>,            // has T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>, // no toml::from<T>
    std::is_default_constructible<T>                // T{} works
    >::value, T>
get(const basic_value<TC>&);

// T(const toml::value&) and T is not toml::basic_value,
// and it does not have `from<T>` nor `from_toml`.
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_constructible<T, const basic_value<TC>&>,   // has T(const basic_value&)
    cxx::negation<detail::is_basic_value<T>>,           // but not basic_value itself
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no .from_toml()
    cxx::negation<detail::has_specialized_from<T>>      // no toml::from<T>
    >::value, T>
get(const basic_value<TC>&);

// ============================================================================
// array-like types; most likely STL container, like std::vector, etc.

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_container<T>,                            // T is a container
    detail::has_push_back_method<T>,                    // .push_back() works
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::array
    cxx::negation<detail::is_std_basic_string<T>>,      // but not std::basic_string<CharT>
#if defined(TOML11_HAS_STRING_VIEW)
    cxx::negation<detail::is_std_basic_string_view<T>>, // but not std::basic_string_view<CharT>
#endif
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;
    const auto& a = v.as_array();

    T container;
    detail::try_reserve(container, a.size()); // if T has .reserve(), call it

    for(const auto& elem : a)
    {
        container.push_back(get<value_type>(elem));
    }
    return container;
}

// ============================================================================
// std::array

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_array<T>::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;
    const auto& a = v.as_array();

    T container;
    if(a.size() != container.size())
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting to an array: "
            " array size is " + std::to_string(container.size()) +
            " but there are " + std::to_string(a.size()) + " elements in toml array.",
            loc, "here"));
    }
    for(std::size_t i=0; i<a.size(); ++i)
    {
        container.at(i) = ::toml::get<value_type>(a.at(i));
    }
    return container;
}

// ============================================================================
// std::forward_list

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_forward_list<T>::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;

    T container;
    for(const auto& elem : v.as_array())
    {
        container.push_front(get<value_type>(elem));
    }
    container.reverse();
    return container;
}

// ============================================================================
// std::pair

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_pair<T>::value, T>
get(const basic_value<TC>& v)
{
    using first_type  = typename T::first_type;
    using second_type = typename T::second_type;

    const auto& ar = v.as_array();
    if(ar.size() != 2)
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting std::pair: "
            " but there are " + std::to_string(ar.size()) + " > 2 elements in toml array.",
            loc, "here"));
    }
    return std::make_pair(::toml::get<first_type >(ar.at(0)),
                          ::toml::get<second_type>(ar.at(1)));
}

// ============================================================================
// std::tuple.

namespace detail
{
template<typename T, typename Array, std::size_t ... I>
T get_tuple_impl(const Array& a, cxx::index_sequence<I...>)
{
    return std::make_tuple(
        ::toml::get<typename std::tuple_element<I, T>::type>(a.at(I))...);
}
} // detail

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_tuple<T>::value, T>
get(const basic_value<TC>& v)
{
    const auto& ar = v.as_array();
    if(ar.size() != std::tuple_size<T>::value)
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting std::tuple: "
            " there are " + std::to_string(ar.size()) + " > " +
            std::to_string(std::tuple_size<T>::value) + " elements in toml array.",
            loc, "here"));
    }
    return detail::get_tuple_impl<T>(ar,
            cxx::make_index_sequence<std::tuple_size<T>::value>{});
}

// ============================================================================
// map-like types; most likely STL map, like std::map or std::unordered_map.

// key is convertible from toml::value::key_type
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    std::is_convertible<typename basic_value<TC>::key_type,
                        typename T::key_type>,          // keys are convertible
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using key_type    = typename T::key_type;
    using mapped_type = typename T::mapped_type;
    static_assert(
        std::is_convertible<typename basic_value<TC>::key_type, key_type>::value,
        "toml::get only supports map type of which key_type is "
        "convertible from toml::basic_value::key_type.");

    T m;
    for(const auto& kv : v.as_table())
    {
        m.emplace(key_type(kv.first), get<mapped_type>(kv.second));
    }
    return m;
}

// key is NOT convertible from toml::value::key_type but std::basic_string
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                       // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,            // but not toml::table
    cxx::negation<std::is_convertible<typename basic_value<TC>::key_type,
        typename T::key_type>>,                              // keys are NOT convertible
    detail::is_1byte_std_basic_string<typename T::key_type>, // is std::basic_string
    cxx::negation<detail::has_from_toml_method<T, TC>>,      // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,          // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using key_type    = typename T::key_type;
    using mapped_type = typename T::mapped_type;

    T m;
    for(const auto& kv : v.as_table())
    {
        m.emplace(detail::string_conv<key_type>(kv.first), get<mapped_type>(kv.second));
    }
    return m;
}

// ============================================================================
// user-defined, but convertible types.

// toml::from<T>
template<typename T, typename TC>
cxx::enable_if_t<detail::has_specialized_from<T>::value, T>
get(const basic_value<TC>& v)
{
    return ::toml::from<T>::from_toml(v);
}

// has T.from_toml(v) but no from<T>
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::has_from_toml_method<T, TC>,            // has T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>, // no toml::from<T>
    std::is_default_constructible<T>                // T{} works
    >::value, T>
get(const basic_value<TC>& v)
{
    T ud;
    ud.from_toml(v);
    return ud;
}

// T(const toml::value&) and T is not toml::basic_value,
// and it does not have `from<T>` nor `from_toml`.
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_constructible<T, const basic_value<TC>&>,   // has T(const basic_value&)
    cxx::negation<detail::is_basic_value<T>>,           // but not basic_value itself
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no .from_toml()
    cxx::negation<detail::has_specialized_from<T>>      // no toml::from<T>
    >::value, T>
get(const basic_value<TC>& v)
{
    return T(v);
}

// ============================================================================
// get_or(value, fallback)

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
get_or(const basic_value<TC>& v, const basic_value<TC>&)
{
    return v;
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
get_or(basic_value<TC>& v, basic_value<TC>&)
{
    return v;
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
get_or(basic_value<TC>&& v, basic_value<TC>&&)
{
    return v;
}

// ----------------------------------------------------------------------------
// specialization for the exact toml types (return type becomes lvalue ref)

template<typename T, typename TC>
cxx::enable_if_t<
    detail::is_exact_toml_type<T, basic_value<TC>>::value, T> const&
get_or(const basic_value<TC>& v, const T& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return opt;
    }
}
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<std::is_const<T>>,
        detail::is_exact_toml_type<T, basic_value<TC>>
    >::value, T>&
get_or(basic_value<TC>& v, T& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return opt;
    }
}
template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<cxx::remove_cvref_t<T>,
    basic_value<TC>>::value, cxx::remove_cvref_t<T>>
get_or(basic_value<TC>&& v, T&& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(std::move(v));
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::forward<T>(opt));
    }
}

// ----------------------------------------------------------------------------
// specialization for string literal

// template<std::size_t N, typename TC>
// typename basic_value<TC>::string_type
// get_or(const basic_value<TC>& v,
//        const typename basic_value<TC>::string_type::value_type (&opt)[N])
// {
//     try
//     {
//         return v.as_string();
//     }
//     catch(...)
//     {
//         return typename basic_value<TC>::string_type(opt);
//     }
// }
//
// The above only matches to the literal, like `get_or(v, "foo");` but not
// ```cpp
// const auto opt = "foo";
// const auto str = get_or(v, opt);
// ```
// . And the latter causes an error.
// To match to both `"foo"` and `const auto opt = "foo"`, we take a pointer to
// a character here.

template<typename TC>
typename basic_value<TC>::string_type
get_or(const basic_value<TC>& v,
       const typename basic_value<TC>::string_type::value_type* opt)
{
    try
    {
        return v.as_string();
    }
    catch(...)
    {
        return typename basic_value<TC>::string_type(opt);
    }
}

// ----------------------------------------------------------------------------
// others (require type conversion and return type cannot be lvalue reference)

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    cxx::negation<detail::is_basic_value<T>>,
    cxx::negation<detail::is_exact_toml_type<T, basic_value<TC>>>,
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, typename basic_value<TC>::string_type::value_type const*>>
    >::value, cxx::remove_cvref_t<T>>
get_or(const basic_value<TC>& v, T&& opt)
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::forward<T>(opt));
    }
}

} // toml
#endif // TOML11_GET_HPP
