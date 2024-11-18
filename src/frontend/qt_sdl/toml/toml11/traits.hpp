#ifndef TOML11_TRAITS_HPP
#define TOML11_TRAITS_HPP

#include "from.hpp"
#include "into.hpp"
#include "compat.hpp"

#include <array>
#include <chrono>
#include <forward_list>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif

namespace toml
{
template<typename TypeConcig>
class basic_value;

namespace detail
{
// ---------------------------------------------------------------------------
// check whether type T is a kind of container/map class

struct has_iterator_impl
{
    template<typename T> static std::true_type  check(typename T::iterator*);
    template<typename T> static std::false_type check(...);
};
struct has_value_type_impl
{
    template<typename T> static std::true_type  check(typename T::value_type*);
    template<typename T> static std::false_type check(...);
};
struct has_key_type_impl
{
    template<typename T> static std::true_type  check(typename T::key_type*);
    template<typename T> static std::false_type check(...);
};
struct has_mapped_type_impl
{
    template<typename T> static std::true_type  check(typename T::mapped_type*);
    template<typename T> static std::false_type check(...);
};
struct has_reserve_method_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>().reserve(std::declval<std::size_t>()))*);
};
struct has_push_back_method_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))*);
};
struct is_comparable_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>() < std::declval<T>())*);
};

struct has_from_toml_method_impl
{
    template<typename T, typename TC>
    static std::true_type  check(
        decltype(std::declval<T>().from_toml(std::declval<::toml::basic_value<TC>>()))*);

    template<typename T, typename TC>
    static std::false_type check(...);
};
struct has_into_toml_method_impl
{
    template<typename T>
    static std::true_type  check(decltype(std::declval<T>().into_toml())*);
    template<typename T>
    static std::false_type check(...);
};

struct has_template_into_toml_method_impl
{
    template<typename T, typename TypeConfig>
    static std::true_type  check(decltype(std::declval<T>().template into_toml<TypeConfig>())*);
    template<typename T, typename TypeConfig>
    static std::false_type check(...);
};

struct has_specialized_from_impl
{
    template<typename T>
    static std::false_type check(...);
    template<typename T, std::size_t S = sizeof(::toml::from<T>)>
    static std::true_type check(::toml::from<T>*);
};
struct has_specialized_into_impl
{
    template<typename T>
    static std::false_type check(...);
    template<typename T, std::size_t S = sizeof(::toml::into<T>)>
    static std::true_type check(::toml::into<T>*);
};


/// Intel C++ compiler can not use decltype in parent class declaration, here
/// is a hack to work around it. https://stackoverflow.com/a/23953090/4692076
#ifdef __INTEL_COMPILER
#define decltype(...) std::enable_if<true, decltype(__VA_ARGS__)>::type
#endif

template<typename T>
struct has_iterator: decltype(has_iterator_impl::check<T>(nullptr)){};
template<typename T>
struct has_value_type: decltype(has_value_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_key_type: decltype(has_key_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_mapped_type: decltype(has_mapped_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_reserve_method: decltype(has_reserve_method_impl::check<T>(nullptr)){};
template<typename T>
struct has_push_back_method: decltype(has_push_back_method_impl::check<T>(nullptr)){};
template<typename T>
struct is_comparable: decltype(is_comparable_impl::check<T>(nullptr)){};

template<typename T, typename TC>
struct has_from_toml_method: decltype(has_from_toml_method_impl::check<T, TC>(nullptr)){};

template<typename T>
struct has_into_toml_method: decltype(has_into_toml_method_impl::check<T>(nullptr)){};

template<typename T, typename TypeConfig>
struct has_template_into_toml_method: decltype(has_template_into_toml_method_impl::check<T, TypeConfig>(nullptr)){};

template<typename T>
struct has_specialized_from: decltype(has_specialized_from_impl::check<T>(nullptr)){};
template<typename T>
struct has_specialized_into: decltype(has_specialized_into_impl::check<T>(nullptr)){};

#ifdef __INTEL_COMPILER
#undef decltype
#endif

// ---------------------------------------------------------------------------
// type checkers

template<typename T> struct is_std_pair_impl : std::false_type{};
template<typename T1, typename T2>
struct is_std_pair_impl<std::pair<T1, T2>> : std::true_type{};
template<typename T>
using is_std_pair = is_std_pair_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_tuple_impl : std::false_type{};
template<typename ... Ts>
struct is_std_tuple_impl<std::tuple<Ts...>> : std::true_type{};
template<typename T>
using is_std_tuple = is_std_tuple_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_array_impl : std::false_type{};
template<typename T, std::size_t N>
struct is_std_array_impl<std::array<T, N>> : std::true_type{};
template<typename T>
using is_std_array = is_std_array_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_forward_list_impl : std::false_type{};
template<typename T>
struct is_std_forward_list_impl<std::forward_list<T>> : std::true_type{};
template<typename T>
using is_std_forward_list = is_std_forward_list_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_basic_string_impl : std::false_type{};
template<typename C, typename T, typename A>
struct is_std_basic_string_impl<std::basic_string<C, T, A>> : std::true_type{};
template<typename T>
using is_std_basic_string = is_std_basic_string_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_1byte_std_basic_string_impl : std::false_type{};
template<typename C, typename T, typename A>
struct is_1byte_std_basic_string_impl<std::basic_string<C, T, A>>
    : std::integral_constant<bool, sizeof(C) == sizeof(char)> {};
template<typename T>
using is_1byte_std_basic_string = is_std_basic_string_impl<cxx::remove_cvref_t<T>>;

#if defined(TOML11_HAS_STRING_VIEW)
template<typename T> struct is_std_basic_string_view_impl : std::false_type{};
template<typename C, typename T>
struct is_std_basic_string_view_impl<std::basic_string_view<C, T>> : std::true_type{};
template<typename T>
using is_std_basic_string_view = is_std_basic_string_view_impl<cxx::remove_cvref_t<T>>;

template<typename V, typename S>
struct is_string_view_of : std::false_type {};
template<typename C, typename T>
struct is_string_view_of<std::basic_string_view<C, T>, std::basic_string<C, T>> : std::true_type {};
#endif

template<typename T> struct is_chrono_duration_impl: std::false_type{};
template<typename Rep, typename Period>
struct is_chrono_duration_impl<std::chrono::duration<Rep, Period>>: std::true_type{};
template<typename T>
using is_chrono_duration = is_chrono_duration_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_map_impl : cxx::conjunction< // map satisfies all the following conditions
    has_iterator<T>,         // has T::iterator
    has_value_type<T>,       // has T::value_type
    has_key_type<T>,         // has T::key_type
    has_mapped_type<T>       // has T::mapped_type
    >{};
template<typename T>
using is_map = is_map_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_container_impl : cxx::conjunction<
    cxx::negation<is_map<T>>,                         // not a map
    cxx::negation<std::is_same<T, std::string>>,      // not a std::string
#ifdef TOML11_HAS_STRING_VIEW
    cxx::negation<std::is_same<T, std::string_view>>, // not a std::string_view
#endif
    has_iterator<T>,                             // has T::iterator
    has_value_type<T>                            // has T::value_type
    >{};
template<typename T>
using is_container = is_container_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_basic_value_impl: std::false_type{};
template<typename TC>
struct is_basic_value_impl<::toml::basic_value<TC>>: std::true_type{};
template<typename T>
using is_basic_value = is_basic_value_impl<cxx::remove_cvref_t<T>>;

}// detail
}//toml
#endif // TOML11_TRAITS_HPP
