#ifndef TOML11_FIND_HPP
#define TOML11_FIND_HPP

#include <algorithm>

#include "get.hpp"
#include "value.hpp"

#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif

namespace toml
{

// ----------------------------------------------------------------------------
// find<T>(value, key);

template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC> const&>()))
find(const basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(v.at(ky));
}

template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(v.at(ky));
}

template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(std::move(v.at(ky)));
}

// ----------------------------------------------------------------------------
// find<T>(value, idx)

template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC> const&>()))
find(const basic_value<TC>& v, const std::size_t idx)
{
    return ::toml::get<T>(v.at(idx));
}
template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))
find(basic_value<TC>& v, const std::size_t idx)
{
    return ::toml::get<T>(v.at(idx));
}
template<typename T, typename TC>
decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))
find(basic_value<TC>&& v, const std::size_t idx)
{
    return ::toml::get<T>(std::move(v.at(idx)));
}

// ----------------------------------------------------------------------------
// find(value, key/idx), w/o conversion

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return v.at(ky);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(basic_value<TC> const& v, const typename basic_value<TC>::key_type& ky)
{
    return v.at(ky);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& ky)
{
    return basic_value<TC>(std::move(v.at(ky)));
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const std::size_t idx)
{
    return v.at(idx);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(basic_value<TC> const& v, const std::size_t idx)
{
    return v.at(idx);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const std::size_t idx)
{
    return basic_value<TC>(std::move(v.at(idx)));
}

// --------------------------------------------------------------------------
// toml::find(toml::value, toml::key, Ts&& ... keys)

namespace detail
{

// It suppresses warnings by -Wsign-conversion when we pass integer literal
// to toml::find. integer literal `0` is deduced as an int, and will be
// converted to std::size_t. This causes sign-conversion.

template<typename TC>
std::size_t key_cast(const std::size_t& v) noexcept
{
    return v;
}
template<typename TC, typename T>
cxx::enable_if_t<std::is_integral<cxx::remove_cvref_t<T>>::value, std::size_t>
key_cast(const T& v) noexcept
{
    return static_cast<std::size_t>(v);
}

// for string-like (string, string literal, string_view)

template<typename TC>
typename basic_value<TC>::key_type const&
key_cast(const typename basic_value<TC>::key_type& v) noexcept
{
    return v;
}
template<typename TC>
typename basic_value<TC>::key_type
key_cast(const typename basic_value<TC>::key_type::value_type* v)
{
    return typename basic_value<TC>::key_type(v);
}
#if defined(TOML11_HAS_STRING_VIEW)
template<typename TC>
typename basic_value<TC>::key_type
key_cast(const std::string_view v)
{
    return typename basic_value<TC>::key_type(v);
}
#endif // string_view

} // detail

// ----------------------------------------------------------------------------
// find(v, keys...)

template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(const basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(std::move(v.at(detail::key_cast<TC>(k1))), detail::key_cast<TC>(k2), ks...);
}

// ----------------------------------------------------------------------------
// find<T>(v, keys...)

template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
decltype(::toml::get<T>(std::declval<const basic_value<TC>&>()))
find(const basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))
find(basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))
find(basic_value<TC>&& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(std::move(v.at(detail::key_cast<TC>(k1))), detail::key_cast<TC>(k2), ks...);
}

// ===========================================================================
// find_or<T>(value, key, fallback)

// ---------------------------------------------------------------------------
// find_or(v, key, other_v)

template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find_or(basic_value<TC>& v, const K& k, basic_value<TC>& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find_or(const basic_value<TC>& v, const K& k, const basic_value<TC>& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find_or(basic_value<TC>&& v, const K& k, basic_value<TC>&& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}

// ---------------------------------------------------------------------------
// toml types (return type can be a reference)

template<typename T, typename TC, typename K>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value,
    cxx::remove_cvref_t<T> const&>
find_or(const basic_value<TC>& v, const K& k, const T& opt)
{
    try
    {
        return ::toml::get<T>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return opt;
    }
}

template<typename T, typename TC, typename K>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<std::is_const<T>>,
        detail::is_exact_toml_type<T, basic_value<TC>>
    >::value, cxx::remove_cvref_t<T>&>
find_or(basic_value<TC>& v, const K& k, T& opt)
{
    try
    {
        return ::toml::get<T>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return opt;
    }
}

template<typename T, typename TC, typename K>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value,
    cxx::remove_cvref_t<T>>
find_or(basic_value<TC>&& v, const K& k, T opt)
{
    try
    {
        return ::toml::get<T>(std::move(v.at(detail::key_cast<TC>(k))));
    }
    catch(...)
    {
        return T(std::move(opt));
    }
}

// ---------------------------------------------------------------------------
// string literal (deduced as std::string)

// XXX to avoid confusion when T is explicitly specified in find_or<T>(),
//     we restrict the string type as std::string.
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, std::string>
find_or(const basic_value<TC>& v, const K& k, const char* opt)
{
    try
    {
        return ::toml::get<std::string>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return std::string(opt);
    }
}

// ---------------------------------------------------------------------------
// other types (requires type conversion and return type cannot be a reference)

template<typename T, typename TC, typename K>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<detail::is_basic_value<cxx::remove_cvref_t<T>>>,
        detail::is_not_toml_type<cxx::remove_cvref_t<T>, basic_value<TC>>,
        cxx::negation<std::is_same<cxx::remove_cvref_t<T>,
            const typename basic_value<TC>::string_type::value_type*>>
    >::value, cxx::remove_cvref_t<T>>
find_or(const basic_value<TC>& v, const K& ky, T opt)
{
    try
    {
        return ::toml::get<cxx::remove_cvref_t<T>>(v.at(detail::key_cast<TC>(ky)));
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::move(opt));
    }
}

// ----------------------------------------------------------------------------
// recursive

namespace detail
{

template<typename ...Ts>
auto last_one(Ts&&... args)
 -> decltype(std::get<sizeof...(Ts)-1>(std::forward_as_tuple(std::forward<Ts>(args)...)))
{
    return std::get<sizeof...(Ts)-1>(std::forward_as_tuple(std::forward<Ts>(args)...));
}

} // detail

template<typename Value, typename K1, typename K2, typename K3, typename ... Ks>
auto find_or(Value&& v, const K1& k1, const K2& k2, K3&& k3, Ks&& ... keys) noexcept
    -> cxx::enable_if_t<
        detail::is_basic_value<cxx::remove_cvref_t<Value>>::value,
        decltype(find_or(v, k2, std::forward<K3>(k3), std::forward<Ks>(keys)...))
    >
{
    try
    {
        return find_or(v.at(k1), k2, std::forward<K3>(k3), std::forward<Ks>(keys)...);
    }
    catch(...)
    {
        return detail::last_one(k3, keys...);
    }
}

template<typename T, typename TC, typename K1, typename K2, typename K3, typename ... Ks>
T find_or(const basic_value<TC>& v, const K1& k1, const K2& k2, const K3& k3, const Ks& ... keys) noexcept
{
    try
    {
        return find_or<T>(v.at(k1), k2, k3, keys...);
    }
    catch(...)
    {
        return static_cast<T>(detail::last_one(k3, keys...));
    }
}

} // toml
#endif // TOML11_FIND_HPP
