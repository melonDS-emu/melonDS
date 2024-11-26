#ifndef TOML11_STORAGE_HPP
#define TOML11_STORAGE_HPP

#include "compat.hpp"

namespace toml
{
namespace detail
{

// It owns a pointer to T. It does deep-copy when copied.
// This struct is introduced to implement a recursive type.
//
// `toml::value` contains `std::vector<toml::value>` to represent a toml array.
// But, in the definition of `toml::value`, `toml::value` is still incomplete.
// `std::vector` of an incomplete type is not allowed in C++11 (it is allowed
// after C++17). To avoid this, we need to use a pointer to `toml::value`, like
// `std::vector<std::unique_ptr<toml::value>>`. Although `std::unique_ptr` is
// noncopyable, we want to make `toml::value` copyable. `storage` is introduced
// to resolve those problems.
template<typename T>
struct storage
{
    using value_type = T;

    explicit storage(value_type v): ptr_(cxx::make_unique<T>(std::move(v))) {}
    ~storage() = default;

    storage(const storage& rhs): ptr_(cxx::make_unique<T>(*rhs.ptr_)) {}
    storage& operator=(const storage& rhs)
    {
        this->ptr_ = cxx::make_unique<T>(*rhs.ptr_);
        return *this;
    }

    storage(storage&&) = default;
    storage& operator=(storage&&) = default;

    bool is_ok() const noexcept {return static_cast<bool>(ptr_);}

    value_type& get() const noexcept {return *ptr_;}

  private:
    std::unique_ptr<value_type> ptr_;
};

} // detail
} // toml
#endif // TOML11_STORAGE_HPP
