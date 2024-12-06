#ifndef TOML11_SCANNER_FWD_HPP
#define TOML11_SCANNER_FWD_HPP

#include "../region.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cctype>

namespace toml
{
namespace detail
{

class scanner_base
{
  public:
    virtual ~scanner_base() = default;
    virtual region scan(location& loc) const = 0;
    virtual scanner_base* clone() const = 0;

    // returns expected character or set of characters or literal.
    // to show the error location, it changes loc (in `sequence`, especially).
    virtual std::string expected_chars(location& loc) const = 0;
    virtual std::string name() const = 0;
};

// make `scanner*` copyable
struct scanner_storage
{
    template<typename Scanner, cxx::enable_if_t<
        std::is_base_of<scanner_base, cxx::remove_cvref_t<Scanner>>::value,
        std::nullptr_t> = nullptr>
    explicit scanner_storage(Scanner&& s)
        : scanner_(cxx::make_unique<cxx::remove_cvref_t<Scanner>>(std::forward<Scanner>(s)))
    {}
    ~scanner_storage() = default;

    scanner_storage(const scanner_storage& other);
    scanner_storage& operator=(const scanner_storage& other);
    scanner_storage(scanner_storage&&) = default;
    scanner_storage& operator=(scanner_storage&&) = default;

    bool is_ok() const noexcept {return static_cast<bool>(scanner_);}

    region scan(location& loc) const;

    std::string expected_chars(location& loc) const;

    scanner_base& get() const noexcept;

    std::string name() const;

  private:

    std::unique_ptr<scanner_base> scanner_;
};

// ----------------------------------------------------------------------------

class character final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit character(const char_type c) noexcept
        : value_(c)
    {}
    ~character() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    char_type value_;
};

// ----------------------------------------------------------------------------

class character_either final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit character_either(std::initializer_list<char_type> cs) noexcept
        : chars_(std::move(cs))
    {
        assert(! this->chars_.empty());
    }

    template<std::size_t N>
    explicit character_either(const char (&cs)[N]) noexcept
        : chars_(N-1, '\0')
    {
        static_assert(N >= 1, "");
        for(std::size_t i=0; i+1<N; ++i)
        {
            chars_.at(i) = char_type(cs[i]);
        }
    }
    ~character_either() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    void push_back(const char_type c);

    std::string name() const override;

  private:
    std::vector<char_type> chars_;
};

// ----------------------------------------------------------------------------

class character_in_range final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit character_in_range(const char_type from, const char_type to) noexcept
        : from_(from), to_(to)
    {}
    ~character_in_range() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    char_type from_;
    char_type to_;
};

// ----------------------------------------------------------------------------

class literal final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    template<std::size_t N>
    explicit literal(const char (&cs)[N]) noexcept
        : value_(cs), size_(N-1) // remove null character at the end
    {}
    ~literal() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    const char* value_;
    std::size_t size_;
};

// ----------------------------------------------------------------------------

class sequence final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename ... Ts>
    explicit sequence(Ts&& ... args)
    {
        push_back_all(std::forward<Ts>(args)...);
    }
    sequence(const sequence&)            = default;
    sequence(sequence&&)                 = default;
    sequence& operator=(const sequence&) = default;
    sequence& operator=(sequence&&)      = default;
    ~sequence() override                 = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    template<typename Scanner>
    void push_back(Scanner&& other_scanner)
    {
        this->others_.emplace_back(std::forward<Scanner>(other_scanner));
    }

    std::string name() const override;

  private:

    void push_back_all()
    {
        return;
    }
    template<typename T, typename ... Ts>
    void push_back_all(T&& head, Ts&& ... args)
    {
        others_.emplace_back(std::forward<T>(head));
        push_back_all(std::forward<Ts>(args)...);
        return;
    }

  private:
    std::vector<scanner_storage> others_;
};

// ----------------------------------------------------------------------------

class either final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename ... Ts>
    explicit either(Ts&& ... args)
    {
        push_back_all(std::forward<Ts>(args)...);
    }
    either(const either&)            = default;
    either(either&&)                 = default;
    either& operator=(const either&) = default;
    either& operator=(either&&)      = default;
    ~either() override               = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    template<typename Scanner>
    void push_back(Scanner&& other_scanner)
    {
        this->others_.emplace_back(std::forward<Scanner>(other_scanner));
    }

    std::string name() const override;

  private:

    void push_back_all()
    {
        return;
    }
    template<typename T, typename ... Ts>
    void push_back_all(T&& head, Ts&& ... args)
    {
        others_.emplace_back(std::forward<T>(head));
        push_back_all(std::forward<Ts>(args)...);
        return;
    }

  private:
    std::vector<scanner_storage> others_;
};

// ----------------------------------------------------------------------------

class repeat_exact final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    repeat_exact(const std::size_t length, Scanner&& other)
        : length_(length), other_(std::forward<Scanner>(other))
    {}
    repeat_exact(const repeat_exact&)            = default;
    repeat_exact(repeat_exact&&)                 = default;
    repeat_exact& operator=(const repeat_exact&) = default;
    repeat_exact& operator=(repeat_exact&&)      = default;
    ~repeat_exact() override                     = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    std::size_t length_;
    scanner_storage other_;
};

// ----------------------------------------------------------------------------

class repeat_at_least final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    repeat_at_least(const std::size_t length, Scanner&& s)
        : length_(length), other_(std::forward<Scanner>(s))
    {}
    repeat_at_least(const repeat_at_least&)            = default;
    repeat_at_least(repeat_at_least&&)                 = default;
    repeat_at_least& operator=(const repeat_at_least&) = default;
    repeat_at_least& operator=(repeat_at_least&&)      = default;
    ~repeat_at_least() override                        = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    std::size_t length_;
    scanner_storage other_;
};

// ----------------------------------------------------------------------------

class maybe final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    explicit maybe(Scanner&& s)
        : other_(std::forward<Scanner>(s))
    {}
    maybe(const maybe&)            = default;
    maybe(maybe&&)                 = default;
    maybe& operator=(const maybe&) = default;
    maybe& operator=(maybe&&)      = default;
    ~maybe() override              = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    scanner_storage other_;
};

} // detail
} // toml
#endif // TOML11_SCANNER_FWD_HPP
