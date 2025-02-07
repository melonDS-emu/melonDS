#ifndef TOML11_RESULT_HPP
#define TOML11_RESULT_HPP

#include "compat.hpp"
#include "exception.hpp"

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include <cassert>

namespace toml
{

struct bad_result_access final : public ::toml::exception
{
  public:
    explicit bad_result_access(std::string what_arg)
        : what_(std::move(what_arg))
    {}
    ~bad_result_access() noexcept override = default;
    const char* what() const noexcept override {return what_.c_str();}

  private:
    std::string what_;
};

// -----------------------------------------------------------------------------

template<typename T>
struct success
{
    static_assert( ! std::is_same<T, void>::value, "");

    using value_type = T;

    explicit success(value_type v)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value(std::move(v))
    {}

    template<typename U, cxx::enable_if_t<
        std::is_convertible<cxx::remove_cvref_t<U>, T>::value,
        std::nullptr_t> = nullptr>
    explicit success(U&& v): value(std::forward<U>(v)) {}

    template<typename U>
    explicit success(success<U> v): value(std::move(v.value)) {}

    ~success() = default;
    success(const success&) = default;
    success(success&&)      = default;
    success& operator=(const success&) = default;
    success& operator=(success&&)      = default;

    value_type&       get()       noexcept {return value;}
    value_type const& get() const noexcept {return value;}

  private:

    value_type value;
};

template<typename T>
struct success<std::reference_wrapper<T>>
{
    static_assert( ! std::is_same<T, void>::value, "");

    using value_type = T;

    explicit success(std::reference_wrapper<value_type> v) noexcept
        : value(std::move(v))
    {}

    ~success() = default;
    success(const success&) = default;
    success(success&&)      = default;
    success& operator=(const success&) = default;
    success& operator=(success&&)      = default;

    value_type&       get()       noexcept {return value.get();}
    value_type const& get() const noexcept {return value.get();}

  private:

    std::reference_wrapper<value_type> value;
};

template<typename T>
success<typename std::decay<T>::type> ok(T&& v)
{
    return success<typename std::decay<T>::type>(std::forward<T>(v));
}
template<std::size_t N>
success<std::string> ok(const char (&literal)[N])
{
    return success<std::string>(std::string(literal));
}

// -----------------------------------------------------------------------------

template<typename T>
struct failure
{
    using value_type = T;

    explicit failure(value_type v)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value(std::move(v))
    {}

    template<typename U, cxx::enable_if_t<
        std::is_convertible<cxx::remove_cvref_t<U>, T>::value,
        std::nullptr_t> = nullptr>
    explicit failure(U&& v): value(std::forward<U>(v)) {}

    template<typename U>
    explicit failure(failure<U> v): value(std::move(v.value)) {}

    ~failure() = default;
    failure(const failure&) = default;
    failure(failure&&)      = default;
    failure& operator=(const failure&) = default;
    failure& operator=(failure&&)      = default;

    value_type&       get()       noexcept {return value;}
    value_type const& get() const noexcept {return value;}

  private:

    value_type value;
};

template<typename T>
struct failure<std::reference_wrapper<T>>
{
    using value_type = T;

    explicit failure(std::reference_wrapper<value_type> v) noexcept
        : value(std::move(v))
    {}

    ~failure() = default;
    failure(const failure&) = default;
    failure(failure&&)      = default;
    failure& operator=(const failure&) = default;
    failure& operator=(failure&&)      = default;

    value_type&       get()       noexcept {return value.get();}
    value_type const& get() const noexcept {return value.get();}

  private:

    std::reference_wrapper<value_type> value;
};

template<typename T>
failure<typename std::decay<T>::type> err(T&& v)
{
    return failure<typename std::decay<T>::type>(std::forward<T>(v));
}

template<std::size_t N>
failure<std::string> err(const char (&literal)[N])
{
    return failure<std::string>(std::string(literal));
}

/* ============================================================================
 *                  _ _
 *  _ _ ___ ____  _| | |_
 * | '_/ -_|_-< || | |  _|
 * |_| \___/__/\_,_|_|\__|
 */

template<typename T, typename E>
struct result
{
    using success_type = success<T>;
    using failure_type = failure<E>;
    using value_type = typename success_type::value_type;
    using error_type = typename failure_type::value_type;

    result(success_type s): is_ok_(true),  succ_(std::move(s)) {}
    result(failure_type f): is_ok_(false), fail_(std::move(f)) {}

    template<typename U, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>
        >::value, std::nullptr_t> = nullptr>
    result(success<U> s): is_ok_(true),  succ_(std::move(s.value)) {}

    template<typename U, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result(failure<U> f): is_ok_(false), fail_(std::move(f.value)) {}

    result& operator=(success_type s)
    {
        this->cleanup();
        this->is_ok_ = true;
        auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(s));
        assert(tmp == std::addressof(this->succ_));
        (void)tmp;
        return *this;
    }
    result& operator=(failure_type f)
    {
        this->cleanup();
        this->is_ok_ = false;
        auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(f));
        assert(tmp == std::addressof(this->fail_));
        (void)tmp;
        return *this;
    }

    template<typename U>
    result& operator=(success<U> s)
    {
        this->cleanup();
        this->is_ok_ = true;
        auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(s.value));
        assert(tmp == std::addressof(this->succ_));
        (void)tmp;
        return *this;
    }
    template<typename U>
    result& operator=(failure<U> f)
    {
        this->cleanup();
        this->is_ok_ = false;
        auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(f.value));
        assert(tmp == std::addressof(this->fail_));
        (void)tmp;
        return *this;
    }

    ~result() noexcept {this->cleanup();}

    result(const result& other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(other.succ_);
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(other.fail_);
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }
    result(result&& other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.succ_));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.fail_));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }

    result& operator=(const result& other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(other.succ_);
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(other.fail_);
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }
    result& operator=(result&& other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.succ_));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.fail_));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }

    template<typename U, typename F, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<F>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>,
            std::is_convertible<cxx::remove_cvref_t<F>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result(result<U, F> other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.as_ok()));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.as_err()));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }

    template<typename U, typename F, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<F>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>,
            std::is_convertible<cxx::remove_cvref_t<F>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result& operator=(result<U, F> other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.as_ok()));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.as_err()));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }

    bool is_ok()  const noexcept {return is_ok_;}
    bool is_err() const noexcept {return !is_ok_;}

    explicit operator bool() const noexcept {return is_ok_;}

    value_type& unwrap(cxx::source_location loc = cxx::source_location::current())
    {
        if(this->is_err())
        {
            throw bad_result_access("toml::result: bad unwrap" + cxx::to_string(loc));
        }
        return this->succ_.get();
    }
    value_type const& unwrap(cxx::source_location loc = cxx::source_location::current()) const
    {
        if(this->is_err())
        {
            throw bad_result_access("toml::result: bad unwrap" + cxx::to_string(loc));
        }
        return this->succ_.get();
    }

    value_type& unwrap_or(value_type& opt) noexcept
    {
        if(this->is_err()) {return opt;}
        return this->succ_.get();
    }
    value_type const& unwrap_or(value_type const& opt) const noexcept
    {
        if(this->is_err()) {return opt;}
        return this->succ_.get();
    }

    error_type& unwrap_err(cxx::source_location loc = cxx::source_location::current())
    {
        if(this->is_ok())
        {
            throw bad_result_access("toml::result: bad unwrap_err" + cxx::to_string(loc));
        }
        return this->fail_.get();
    }
    error_type const& unwrap_err(cxx::source_location loc = cxx::source_location::current()) const
    {
        if(this->is_ok())
        {
            throw bad_result_access("toml::result: bad unwrap_err" + cxx::to_string(loc));
        }
        return this->fail_.get();
    }

    value_type& as_ok() noexcept
    {
        assert(this->is_ok());
        return this->succ_.get();
    }
    value_type const& as_ok() const noexcept
    {
        assert(this->is_ok());
        return this->succ_.get();
    }

    error_type& as_err() noexcept
    {
        assert(this->is_err());
        return this->fail_.get();
    }
    error_type const& as_err() const noexcept
    {
        assert(this->is_err());
        return this->fail_.get();
    }

  private:

    void cleanup() noexcept
    {
#if defined(__GNUC__) && ! defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#endif

        if(this->is_ok_) {this->succ_.~success_type();}
        else             {this->fail_.~failure_type();}

#if defined(__GNUC__) && ! defined(__clang__)
#pragma GCC diagnostic pop
#endif
        return;
    }

  private:

    bool      is_ok_;
    union
    {
        success_type succ_;
        failure_type fail_;
    };
};

// ----------------------------------------------------------------------------

namespace detail
{
struct none_t {};
inline bool operator==(const none_t&, const none_t&) noexcept {return true;}
inline bool operator!=(const none_t&, const none_t&) noexcept {return false;}
inline bool operator< (const none_t&, const none_t&) noexcept {return false;}
inline bool operator<=(const none_t&, const none_t&) noexcept {return true;}
inline bool operator> (const none_t&, const none_t&) noexcept {return false;}
inline bool operator>=(const none_t&, const none_t&) noexcept {return true;}
inline std::ostream& operator<<(std::ostream& os, const none_t&)
{
    os << "none";
    return os;
}
} // detail

inline success<detail::none_t> ok() noexcept
{
    return success<detail::none_t>(detail::none_t{});
}
inline failure<detail::none_t> err() noexcept
{
    return failure<detail::none_t>(detail::none_t{});
}

} // toml
#endif // TOML11_RESULT_HPP
