#ifndef TOML11_COMPAT_HPP
#define TOML11_COMPAT_HPP

#include "version.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

#include <cassert>

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if __has_include(<bit>)
#    include <bit>
#  endif
#endif

#include <cstring>

// ----------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if __has_cpp_attribute(deprecated)
#    define TOML11_HAS_ATTR_DEPRECATED 1
#  endif
#endif

#if defined(TOML11_HAS_ATTR_DEPRECATED)
#  define TOML11_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(__GNUC__)
#  define TOML11_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#  define TOML11_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#  define TOML11_DEPRECATED(msg)
#endif

// ----------------------------------------------------------------------------

#if defined(__cpp_if_constexpr)
#  if __cpp_if_constexpr >= 201606L
#    define TOML11_HAS_CONSTEXPR_IF 1
#  endif
#endif

#if defined(TOML11_HAS_CONSTEXPR_IF)
#  define TOML11_CONSTEXPR_IF if constexpr
#else
#  define TOML11_CONSTEXPR_IF if
#endif

// ----------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_make_unique)
#    if __cpp_lib_make_unique >= 201304L
#      define TOML11_HAS_STD_MAKE_UNIQUE 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{

#if defined(TOML11_HAS_STD_MAKE_UNIQUE)

using std::make_unique;

#else

template<typename T, typename ... Ts>
std::unique_ptr<T> make_unique(Ts&& ... args)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(args)...));
}

#endif // TOML11_HAS_STD_MAKE_UNIQUE

} // cxx
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_make_reverse_iterator)
#    if __cpp_lib_make_reverse_iterator >= 201402L
#      define TOML11_HAS_STD_MAKE_REVERSE_ITERATOR 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
# if defined(TOML11_HAS_STD_MAKE_REVERSE_ITERATOR)

using std::make_reverse_iterator;

#else

template<typename Iterator>
std::reverse_iterator<Iterator> make_reverse_iterator(Iterator iter)
{
    return std::reverse_iterator<Iterator>(iter);
}

#endif // TOML11_HAS_STD_MAKE_REVERSE_ITERATOR

} // cxx
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_clamp)
#    if __cpp_lib_clamp >= 201603L
#      define TOML11_HAS_STD_CLAMP 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_CLAMP)

using std::clamp;

#else

template<typename T>
T clamp(const T& x, const T& low, const T& high) noexcept
{
    assert(low <= high);
    return (std::min)((std::max)(x, low), high);
}

#endif // TOML11_HAS_STD_CLAMP

} // cxx
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_bit_cast)
#    if __cpp_lib_bit_cast >= 201806L
#      define TOML11_HAS_STD_BIT_CAST 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_BIT_CAST)

using std::bit_cast;

#else

template<typename U, typename T>
U bit_cast(const T& x) noexcept
{
    static_assert(sizeof(T) == sizeof(U), "");
    static_assert(std::is_default_constructible<T>::value, "");

    U z;
    std::memcpy(reinterpret_cast<char*>(std::addressof(z)),
                reinterpret_cast<const char*>(std::addressof(x)),
                sizeof(T));

    return z;
}

#endif // TOML11_HAS_STD_BIT_CAST

} // cxx
} // toml

// ---------------------------------------------------------------------------
// C++20 remove_cvref_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_remove_cvref)
#    if __cpp_lib_remove_cvref >= 201711L
#      define TOML11_HAS_STD_REMOVE_CVREF 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_REMOVE_CVREF)

using std::remove_cvref;
using std::remove_cvref_t;

#else

template<typename T>
struct remove_cvref
{
    using type = typename std::remove_cv<
        typename std::remove_reference<T>::type>::type;
};

template<typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

#endif // TOML11_HAS_STD_REMOVE_CVREF

} // cxx
} // toml

// ---------------------------------------------------------------------------
// C++17 and/or/not

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_logical_traits)
#    if __cpp_lib_logical_traits >= 201510L
#      define TOML11_HAS_STD_CONJUNCTION 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_CONJUNCTION)

using std::conjunction;
using std::disjunction;
using std::negation;

#else

template<typename ...> struct conjunction : std::true_type{};
template<typename T>   struct conjunction<T> : T{};
template<typename T, typename ... Ts>
struct conjunction<T, Ts...> :
    std::conditional<static_cast<bool>(T::value), conjunction<Ts...>, T>::type
{};

template<typename ...> struct disjunction : std::false_type{};
template<typename T>   struct disjunction<T> : T {};
template<typename T, typename ... Ts>
struct disjunction<T, Ts...> :
    std::conditional<static_cast<bool>(T::value), T, disjunction<Ts...>>::type
{};

template<typename T>
struct negation : std::integral_constant<bool, !static_cast<bool>(T::value)>{};

#endif // TOML11_HAS_STD_CONJUNCTION

} // cxx
} // toml

// ---------------------------------------------------------------------------
// C++14 index_sequence

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_integer_sequence)
#    if __cpp_lib_integer_sequence >= 201304L
#      define TOML11_HAS_STD_INTEGER_SEQUENCE 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_INTEGER_SEQUENCE)

using std::index_sequence;
using std::make_index_sequence;

#else

template<std::size_t ... Ns> struct index_sequence{};

template<bool B, std::size_t N, typename T>
struct double_index_sequence;

template<std::size_t N, std::size_t ... Is>
struct double_index_sequence<true, N, index_sequence<Is...>>
{
    using type = index_sequence<Is..., (Is+N)..., N*2>;
};
template<std::size_t N, std::size_t ... Is>
struct double_index_sequence<false, N, index_sequence<Is...>>
{
    using type = index_sequence<Is..., (Is+N)...>;
};

template<std::size_t N>
struct index_sequence_maker
{
    using type = typename double_index_sequence<
            N % 2 == 1, N/2, typename index_sequence_maker<N/2>::type
        >::type;
};
template<>
struct index_sequence_maker<0>
{
    using type = index_sequence<>;
};

template<std::size_t N>
using make_index_sequence = typename index_sequence_maker<N>::type;

#endif // TOML11_HAS_STD_INTEGER_SEQUENCE

} // cxx
} // toml

// ---------------------------------------------------------------------------
// C++14 enable_if_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_transformation_trait_aliases)
#    if __cpp_lib_transformation_trait_aliases >= 201304L
#      define TOML11_HAS_STD_ENABLE_IF_T 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_ENABLE_IF_T)

using std::enable_if_t;

#else

template<bool B, typename T>
using enable_if_t = typename std::enable_if<B, T>::type;

#endif // TOML11_HAS_STD_ENABLE_IF_T

} // cxx
} // toml

// ---------------------------------------------------------------------------
// return_type_of_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_is_invocable)
#    if __cpp_lib_is_invocable >= 201703
#      define TOML11_HAS_STD_INVOKE_RESULT 1
#    endif
#  endif
#endif

namespace toml
{
namespace cxx
{
#if defined(TOML11_HAS_STD_INVOKE_RESULT)

template<typename F, typename ... Args>
using return_type_of_t = std::invoke_result_t<F, Args...>;

#else

// result_of is deprecated after C++17
template<typename F, typename ... Args>
using return_type_of_t = typename std::result_of<F(Args...)>::type;

#endif // TOML11_HAS_STD_INVOKE_RESULT

} // cxx
} // toml

// ----------------------------------------------------------------------------
// (subset of) source_location

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= 202002L
#  if __has_include(<source_location>)
#    define TOML11_HAS_STD_SOURCE_LOCATION
#  endif // has_include
#endif // c++20

#if ! defined(TOML11_HAS_STD_SOURCE_LOCATION)
#  if defined(__GNUC__) && ! defined(__clang__)
#    if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#      if __has_include(<experimental/source_location>)
#        define TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION
#      endif
#    endif
#  endif // GNU g++
#endif // not TOML11_HAS_STD_SOURCE_LOCATION

#if ! defined(TOML11_HAS_STD_SOURCE_LOCATION) && ! defined(TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION)
#  if defined(__GNUC__) && ! defined(__clang__)
#    if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE int
#    endif
#  elif defined(__clang__) // clang 9.0.0 implements builtin_FILE/LINE
#    if __has_builtin(__builtin_FILE) && __has_builtin(__builtin_LINE)
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE unsigned int
#    endif
#  elif defined(_MSVC_LANG) && defined(_MSC_VER)
#    if _MSC_VER > 1926
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE int
#    endif
#  endif
#endif

#if defined(TOML11_HAS_STD_SOURCE_LOCATION)
#include <source_location>
namespace toml
{
namespace cxx
{
using source_location = std::source_location;

inline std::string to_string(const source_location& loc)
{
    return std::string(" at line ") + std::to_string(loc.line()) +
           std::string(" in file ") + std::string(loc.file_name());
}
} // cxx
} // toml
#elif defined(TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION)
#include <experimental/source_location>
namespace toml
{
namespace cxx
{
using source_location = std::experimental::source_location;

inline std::string to_string(const source_location& loc)
{
    return std::string(" at line ") + std::to_string(loc.line()) +
           std::string(" in file ") + std::string(loc.file_name());
}
} // cxx
} // toml
#elif defined(TOML11_HAS_BUILTIN_FILE_LINE)
namespace toml
{
namespace cxx
{
struct source_location
{
    using line_type = TOML11_BUILTIN_LINE_TYPE;
    static source_location current(const line_type line = __builtin_LINE(),
                                   const char*     file = __builtin_FILE())
    {
        return source_location(line, file);
    }

    source_location(const line_type line, const char* file)
        : line_(line), file_name_(file)
    {}

    line_type   line()      const noexcept {return line_;}
    const char* file_name() const noexcept {return file_name_;}

  private:

    line_type   line_;
    const char* file_name_;
};

inline std::string to_string(const source_location& loc)
{
    return std::string(" at line ") + std::to_string(loc.line()) +
           std::string(" in file ") + std::string(loc.file_name());
}
} // cxx
} // toml
#else // no builtin
namespace toml
{
namespace cxx
{
struct source_location
{
    static source_location current() { return source_location{}; }
};

inline std::string to_string(const source_location&)
{
    return std::string("");
}
} // cxx
} // toml
#endif // TOML11_HAS_STD_SOURCE_LOCATION

// ----------------------------------------------------------------------------
// (subset of) optional

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<optional>)
#    include <optional>
#  endif // has_include(optional)
#endif // C++17

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_optional)
#    if __cpp_lib_optional >= 201606L
#      define TOML11_HAS_STD_OPTIONAL 1
#    endif
#  endif
#endif

#if defined(TOML11_HAS_STD_OPTIONAL)

namespace toml
{
namespace cxx
{
using std::optional;

inline std::nullopt_t make_nullopt() {return std::nullopt;}

template<typename charT, typename traitsT>
std::basic_ostream<charT, traitsT>&
operator<<(std::basic_ostream<charT, traitsT>& os, const std::nullopt_t&)
{
    os << "nullopt";
    return os;
}

} // cxx
} // toml

#else // TOML11_HAS_STD_OPTIONAL

namespace toml
{
namespace cxx
{

struct nullopt_t{};
inline nullopt_t make_nullopt() {return nullopt_t{};}

inline bool operator==(const nullopt_t&, const nullopt_t&) noexcept {return true;}
inline bool operator!=(const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator< (const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator<=(const nullopt_t&, const nullopt_t&) noexcept {return true;}
inline bool operator> (const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator>=(const nullopt_t&, const nullopt_t&) noexcept {return true;}

template<typename charT, typename traitsT>
std::basic_ostream<charT, traitsT>&
operator<<(std::basic_ostream<charT, traitsT>& os, const nullopt_t&)
{
    os << "nullopt";
    return os;
}

template<typename T>
class optional
{
  public:

    using value_type = T;

  public:

    optional() noexcept : has_value_(false), null_('\0') {}
    optional(nullopt_t) noexcept : has_value_(false), null_('\0') {}

    optional(const T& x): has_value_(true), value_(x) {}
    optional(T&& x): has_value_(true), value_(std::move(x)) {}

    template<typename U, enable_if_t<std::is_constructible<T, U>::value, std::nullptr_t> = nullptr>
    explicit optional(U&& x): has_value_(true), value_(std::forward<U>(x)) {}

    optional(const optional& rhs): has_value_(rhs.has_value_)
    {
        if(rhs.has_value_)
        {
            this->assigner(rhs.value_);
        }
    }
    optional(optional&& rhs): has_value_(rhs.has_value_)
    {
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
    }

    optional& operator=(const optional& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(rhs.value_);
        }
        return *this;
    }
    optional& operator=(optional&& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
        return *this;
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    explicit optional(const optional<U>& rhs): has_value_(rhs.has_value_), null_('\0')
    {
        if(rhs.has_value_)
        {
            this->assigner(rhs.value_);
        }
    }
    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    explicit optional(optional<U>&& rhs): has_value_(rhs.has_value_), null_('\0')
    {
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    optional& operator=(const optional<U>& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(rhs.value_);
        }
        return *this;
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    optional& operator=(optional<U>&& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
        return *this;
    }
    ~optional() noexcept
    {
        this->cleanup();
    }

    explicit operator bool() const noexcept
    {
        return has_value_;
    }

    bool has_value() const noexcept {return has_value_;}

    value_type const& value(source_location loc = source_location::current()) const
    {
        if( ! this->has_value_)
        {
            throw std::runtime_error("optional::value(): bad_unwrap" + to_string(loc));
        }
        return this->value_;
    }
    value_type& value(source_location loc = source_location::current())
    {
        if( ! this->has_value_)
        {
            throw std::runtime_error("optional::value(): bad_unwrap" + to_string(loc));
        }
        return this->value_;
    }

    value_type const& value_or(const value_type& opt) const
    {
        if(this->has_value_) {return this->value_;} else {return opt;}
    }
    value_type& value_or(value_type& opt)
    {
        if(this->has_value_) {return this->value_;} else {return opt;}
    }

  private:

    void cleanup() noexcept
    {
        if(this->has_value_)
        {
            value_.~T();
        }
    }

    template<typename U>
    void assigner(U&& x)
    {
        const auto tmp = ::new(std::addressof(this->value_)) value_type(std::forward<U>(x));
        assert(tmp == std::addressof(this->value_));
        (void)tmp;
    }

  private:

    bool has_value_;
    union
    {
        char null_;
        T    value_;
    };
};
} // cxx
} // toml
#endif // TOML11_HAS_STD_OPTIONAL

#endif // TOML11_COMPAT_HPP
