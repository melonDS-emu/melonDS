#ifndef TOML11_CONVERSION_HPP
#define TOML11_CONVERSION_HPP

#include "find.hpp"
#include "from.hpp" // IWYU pragma: keep
#include "into.hpp" // IWYU pragma: keep

#if defined(TOML11_HAS_OPTIONAL)

#include <optional>

namespace toml
{
namespace detail
{

template<typename T>
inline constexpr bool is_optional_v = false;

template<typename T>
inline constexpr bool is_optional_v<std::optional<T>> = true;

template<typename T, typename TC>
void find_member_variable_from_value(T& obj, const basic_value<TC>& v, const char* var_name)
{
    if constexpr(is_optional_v<T>)
    {
        if(v.contains(var_name))
        {
            obj = toml::find<typename T::value_type>(v, var_name);
        }
        else
        {
            obj = std::nullopt;
        }
    }
    else
    {
        obj = toml::find<T>(v, var_name);
    }
}

template<typename T, typename TC>
void assign_member_variable_to_value(const T& obj, basic_value<TC>& v, const char* var_name)
{
    if constexpr(is_optional_v<T>)
    {
        if(obj.has_value())
        {
            v[var_name] = obj.value();
        }
    }
    else
    {
        v[var_name] = obj;
    }
}

} // detail
} // toml

#else

namespace toml
{
namespace detail
{

template<typename T, typename TC>
void find_member_variable_from_value(T& obj, const basic_value<TC>& v, const char* var_name)
{
    obj = toml::find<T>(v, var_name);
}

template<typename T, typename TC>
void assign_member_variable_to_value(const T& obj, basic_value<TC>& v, const char* var_name)
{
    v[var_name] = obj;
}

} // detail
} // toml

#endif // optional

// use it in the following way.
// ```cpp
// namespace foo
// {
// struct Foo
// {
//     std::string s;
//     double      d;
//     int         i;
// };
// } // foo
//
// TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(foo::Foo, s, d, i)
// ```
//
// And then you can use `toml::get<foo::Foo>(v)` and `toml::find<foo::Foo>(file, "foo");`
//

#define TOML11_STRINGIZE_AUX(x) #x
#define TOML11_STRINGIZE(x)     TOML11_STRINGIZE_AUX(x)

#define TOML11_CONCATENATE_AUX(x, y) x##y
#define TOML11_CONCATENATE(x, y)     TOML11_CONCATENATE_AUX(x, y)

// ============================================================================
// TOML11_DEFINE_CONVERSION_NON_INTRUSIVE

#ifndef TOML11_WITHOUT_DEFINE_NON_INTRUSIVE

// ----------------------------------------------------------------------------
// TOML11_ARGS_SIZE

#define TOML11_INDEX_RSEQ() \
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0
#define TOML11_ARGS_SIZE_IMPL(\
    ARG1,  ARG2,  ARG3,  ARG4,  ARG5,  ARG6,  ARG7,  ARG8,  ARG9,  ARG10, \
    ARG11, ARG12, ARG13, ARG14, ARG15, ARG16, ARG17, ARG18, ARG19, ARG20, \
    ARG21, ARG22, ARG23, ARG24, ARG25, ARG26, ARG27, ARG28, ARG29, ARG30, \
    ARG31, ARG32, N, ...) N
#define TOML11_ARGS_SIZE_AUX(...) TOML11_ARGS_SIZE_IMPL(__VA_ARGS__)
#define TOML11_ARGS_SIZE(...) TOML11_ARGS_SIZE_AUX(__VA_ARGS__, TOML11_INDEX_RSEQ())

// ----------------------------------------------------------------------------
// TOML11_FOR_EACH_VA_ARGS

#define TOML11_FOR_EACH_VA_ARGS_AUX_1( FUNCTOR, ARG1     ) FUNCTOR(ARG1)
#define TOML11_FOR_EACH_VA_ARGS_AUX_2( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_1( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_3( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_2( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_4( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_3( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_5( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_4( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_6( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_5( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_7( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_6( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_8( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_7( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_9( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_8( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_10(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_9( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_11(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_10(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_12(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_11(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_13(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_12(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_14(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_13(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_15(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_14(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_16(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_15(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_17(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_16(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_18(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_17(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_19(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_18(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_20(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_19(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_21(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_20(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_22(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_21(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_23(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_22(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_24(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_23(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_25(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_24(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_26(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_25(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_27(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_26(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_28(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_27(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_29(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_28(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_30(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_29(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_31(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_30(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_32(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_31(FUNCTOR, __VA_ARGS__)

#define TOML11_FOR_EACH_VA_ARGS(FUNCTOR, ...)\
    TOML11_CONCATENATE(TOML11_FOR_EACH_VA_ARGS_AUX_, TOML11_ARGS_SIZE(__VA_ARGS__))(FUNCTOR, __VA_ARGS__)


#define TOML11_FIND_MEMBER_VARIABLE_FROM_VALUE(VAR_NAME)\
    toml::detail::find_member_variable_from_value(obj.VAR_NAME, v, TOML11_STRINGIZE(VAR_NAME));

#define TOML11_ASSIGN_MEMBER_VARIABLE_TO_VALUE(VAR_NAME)\
    toml::detail::assign_member_variable_to_value(obj.VAR_NAME, v, TOML11_STRINGIZE(VAR_NAME));

#define TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(NAME, ...)\
    namespace toml {                                                                     \
    template<>                                                                           \
    struct from<NAME>                                                                    \
    {                                                                                    \
        template<typename TC>                                                            \
        static NAME from_toml(const basic_value<TC>& v)                                  \
        {                                                                                \
            NAME obj;                                                                    \
            TOML11_FOR_EACH_VA_ARGS(TOML11_FIND_MEMBER_VARIABLE_FROM_VALUE, __VA_ARGS__) \
            return obj;                                                                  \
        }                                                                                \
    };                                                                                   \
    template<>                                                                           \
    struct into<NAME>                                                                    \
    {                                                                                    \
        template<typename TC>                                                            \
        static basic_value<TC> into_toml(const NAME& obj)                                \
        {                                                                                \
            ::toml::basic_value<TC> v = typename ::toml::basic_value<TC>::table_type{};  \
            TOML11_FOR_EACH_VA_ARGS(TOML11_ASSIGN_MEMBER_VARIABLE_TO_VALUE, __VA_ARGS__) \
            return v;                                                                    \
        }                                                                                \
    };                                                                                   \
    } /* toml */

#endif// TOML11_WITHOUT_DEFINE_NON_INTRUSIVE

#endif // TOML11_CONVERSION_HPP
