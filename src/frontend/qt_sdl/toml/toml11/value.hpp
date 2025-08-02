#ifndef TOML11_VALUE_HPP
#define TOML11_VALUE_HPP

#include "color.hpp"
#include "datetime.hpp"
#include "exception.hpp"
#include "error_info.hpp"
#include "format.hpp"
#include "region.hpp"
#include "source_location.hpp"
#include "storage.hpp"
#include "traits.hpp"
#include "value_t.hpp"
#include "version.hpp" // IWYU pragma: keep < TOML11_HAS_STRING_VIEW

#ifdef TOML11_HAS_STRING_VIEW
#include <string_view>
#endif

#include <cassert>

namespace toml
{
template<typename TypeConfig>
class basic_value;

struct type_error final : public ::toml::exception
{
  public:
    type_error(std::string what_arg, source_location loc)
        : what_(std::move(what_arg)), loc_(std::move(loc))
    {}
    ~type_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    source_location const& location() const noexcept {return loc_;}

  private:
    std::string what_;
    source_location loc_;
};

// only for internal use
namespace detail
{
template<typename TC>
error_info make_type_error(const basic_value<TC>&, const std::string&, const value_t);

template<typename TC>
error_info make_not_found_error(const basic_value<TC>&, const std::string&, const typename basic_value<TC>::key_type&);

template<typename TC>
void change_region_of_value(basic_value<TC>&, const basic_value<TC>&);

template<typename TC, value_t V>
struct getter;
} // detail

template<typename TypeConfig>
class basic_value
{
  public:

    using config_type          = TypeConfig;
    using key_type             = typename config_type::string_type;
    using value_type           = basic_value<config_type>;
    using boolean_type         = typename config_type::boolean_type;
    using integer_type         = typename config_type::integer_type;
    using floating_type        = typename config_type::floating_type;
    using string_type          = typename config_type::string_type;
    using local_time_type      = ::toml::local_time;
    using local_date_type      = ::toml::local_date;
    using local_datetime_type  = ::toml::local_datetime;
    using offset_datetime_type = ::toml::offset_datetime;
    using array_type           = typename config_type::template array_type<value_type>;
    using table_type           = typename config_type::template table_type<key_type, value_type>;
    using comment_type         = typename config_type::comment_type;
    using char_type            = typename string_type::value_type;

  private:

    using region_type = detail::region;

  public:

    basic_value() noexcept
        : type_(value_t::empty), empty_('\0'), region_{}, comments_{}
    {}
    ~basic_value() noexcept {this->cleanup();}

    // copy/move constructor/assigner ===================================== {{{

    basic_value(const basic_value& v)
        : type_(v.type_), region_(v.region_), comments_(v.comments_)
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , v.boolean_        ); break;
            case value_t::integer        : assigner(integer_        , v.integer_        ); break;
            case value_t::floating       : assigner(floating_       , v.floating_       ); break;
            case value_t::string         : assigner(string_         , v.string_         ); break;
            case value_t::offset_datetime: assigner(offset_datetime_, v.offset_datetime_); break;
            case value_t::local_datetime : assigner(local_datetime_ , v.local_datetime_ ); break;
            case value_t::local_date     : assigner(local_date_     , v.local_date_     ); break;
            case value_t::local_time     : assigner(local_time_     , v.local_time_     ); break;
            case value_t::array          : assigner(array_          , v.array_          ); break;
            case value_t::table          : assigner(table_          , v.table_          ); break;
            default                      : assigner(empty_          , '\0'              ); break;
        }
    }
    basic_value(basic_value&& v)
        : type_(v.type()), region_(std::move(v.region_)),
          comments_(std::move(v.comments_))
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
    }

    basic_value& operator=(const basic_value& v)
    {
        if(this == std::addressof(v)) {return *this;}

        this->cleanup();
        this->type_     = v.type_;
        this->region_   = v.region_;
        this->comments_ = v.comments_;
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , v.boolean_        ); break;
            case value_t::integer        : assigner(integer_        , v.integer_        ); break;
            case value_t::floating       : assigner(floating_       , v.floating_       ); break;
            case value_t::string         : assigner(string_         , v.string_         ); break;
            case value_t::offset_datetime: assigner(offset_datetime_, v.offset_datetime_); break;
            case value_t::local_datetime : assigner(local_datetime_ , v.local_datetime_ ); break;
            case value_t::local_date     : assigner(local_date_     , v.local_date_     ); break;
            case value_t::local_time     : assigner(local_time_     , v.local_time_     ); break;
            case value_t::array          : assigner(array_          , v.array_          ); break;
            case value_t::table          : assigner(table_          , v.table_          ); break;
            default                      : assigner(empty_          , '\0'              ); break;
        }
        return *this;
    }
    basic_value& operator=(basic_value&& v)
    {
        if(this == std::addressof(v)) {return *this;}

        this->cleanup();
        this->type_     = v.type_;
        this->region_   = std::move(v.region_);
        this->comments_ = std::move(v.comments_);
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
        return *this;
    }
    // }}}

    // constructor to overwrite commnets ================================== {{{

    basic_value(basic_value v, std::vector<std::string> com)
        : type_(v.type()), region_(std::move(v.region_)),
          comments_(std::move(com))
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
    }
    // }}}

    // conversion between different basic_values ========================== {{{

    template<typename TI>
    basic_value(basic_value<TI> other)
        : type_(other.type_),
          region_(std::move(other.region_)),
          comments_(std::move(other.comments_))
    {
        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
    }

    template<typename TI>
    basic_value(basic_value<TI> other, std::vector<std::string> com)
        : type_(other.type_),
          region_(std::move(other.region_)),
          comments_(std::move(com))
    {
        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
    }
    template<typename TI>
    basic_value& operator=(basic_value<TI> other)
    {
        this->cleanup();
        this->region_ = other.region_;
        this->comments_    = comment_type(other.comments_);
        this->type_        = other.type_;
        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
        return *this;
    }
    // }}}

    // constructor (boolean) ============================================== {{{

    basic_value(boolean_type x)
        : basic_value(x, boolean_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(boolean_type x, std::vector<std::string> com)
        : basic_value(x, boolean_format_info{}, std::move(com), region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::boolean), boolean_(boolean_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(boolean_type x)
    {
        boolean_format_info fmt;
        if(this->is_boolean())
        {
            fmt = this->as_boolean_fmt();
        }
        this->cleanup();
        this->type_   = value_t::boolean;
        this->region_ = region_type{};
        assigner(this->boolean_, boolean_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (integer) ============================================== {{{

    basic_value(integer_type x)
        : basic_value(std::move(x), integer_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(integer_type x, std::vector<std::string> com)
        : basic_value(std::move(x), integer_format_info{}, std::move(com), region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::integer), integer_(integer_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(integer_type x)
    {
        integer_format_info fmt;
        if(this->is_integer())
        {
            fmt = this->as_integer_fmt();
        }
        this->cleanup();
        this->type_   = value_t::integer;
        this->region_ = region_type{};
        assigner(this->integer_, integer_storage(std::move(x), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_integer_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, boolean_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, integer_type>>,
            std::is_integral<cxx::remove_cvref_t<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), integer_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), integer_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::integer), integer_(integer_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        integer_format_info fmt;
        if(this->is_integer())
        {
            fmt = this->as_integer_fmt();
        }
        this->cleanup();
        this->type_   = value_t::integer;
        this->region_ = region_type{};
        assigner(this->integer_, integer_storage(x, std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (floating) ============================================= {{{

    basic_value(floating_type x)
        : basic_value(std::move(x), floating_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(floating_type x, std::vector<std::string> com)
        : basic_value(std::move(x), floating_format_info{}, std::move(com), region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::floating), floating_(floating_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(floating_type x)
    {
        floating_format_info fmt;
        if(this->is_floating())
        {
            fmt = this->as_floating_fmt();
        }
        this->cleanup();
        this->type_   = value_t::floating;
        this->region_ = region_type{};
        assigner(this->floating_, floating_storage(std::move(x), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_floating_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, floating_type>>,
            std::is_floating_point<cxx::remove_cvref_t<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(x, floating_format_info{}, std::vector<std::string>{}, region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(x, floating_format_info{}, std::move(com), region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::floating), floating_(floating_storage(x, std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        floating_format_info fmt;
        if(this->is_floating())
        {
            fmt = this->as_floating_fmt();
        }
        this->cleanup();
        this->type_   = value_t::floating;
        this->region_ = region_type{};
        assigner(this->floating_, floating_storage(x, std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (string) =============================================== {{{

    basic_value(string_type x)
        : basic_value(std::move(x), string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_type x, string_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_type x, std::vector<std::string> com)
        : basic_value(std::move(x), string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(string_type x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(string_type x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(string_type x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
        assigner(this->string_, string_storage(x, std::move(fmt)));
        return *this;
    }

    // "string literal"

    basic_value(const typename string_type::value_type* x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(const typename string_type::value_type* x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(string_type(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(const typename string_type::value_type* x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
        assigner(this->string_, string_storage(string_type(x), std::move(fmt)));
        return *this;
    }

#if defined(TOML11_HAS_STRING_VIEW)
    using string_view_type = std::basic_string_view<
        typename string_type::value_type, typename string_type::traits_type>;

    basic_value(string_view_type x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_view_type x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(string_type(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(string_view_type x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
        assigner(this->string_, string_storage(string_type(x), std::move(fmt)));
        return *this;
    }

#endif // TOML11_HAS_STRING_VIEW

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string),
          string_(string_storage(detail::string_conv<string_type>(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
        assigner(this->string_, string_storage(detail::string_conv<string_type>(x), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (local_date) =========================================== {{{

    basic_value(local_date_type x)
        : basic_value(x, local_date_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_date_type x, std::vector<std::string> com)
        : basic_value(x, local_date_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_date), local_date_(local_date_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(local_date_type x)
    {
        local_date_format_info fmt;
        if(this->is_local_date())
        {
            fmt = this->as_local_date_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_date;
        this->region_ = region_type{};
        assigner(this->local_date_, local_date_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (local_time) =========================================== {{{

    basic_value(local_time_type x)
        : basic_value(x, local_time_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_time_type x, std::vector<std::string> com)
        : basic_value(x, local_time_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_time), local_time_(local_time_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(local_time_type x)
    {
        local_time_format_info fmt;
        if(this->is_local_time())
        {
            fmt = this->as_local_time_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_time;
        this->region_ = region_type{};
        assigner(this->local_time_, local_time_storage(x, fmt));
        return *this;
    }

    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x)
        : basic_value(local_time_type(x), local_time_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, local_time_format_info fmt)
        : basic_value(local_time_type(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, std::vector<std::string> com)
        : basic_value(local_time_type(x), local_time_format_info{}, std::move(com), region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, local_time_format_info fmt, std::vector<std::string> com)
        : basic_value(local_time_type(x), std::move(fmt), std::move(com), region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x,
                local_time_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : basic_value(local_time_type(x), std::move(fmt), std::move(com), std::move(reg))
    {}
    template<typename Rep, typename Period>
    basic_value& operator=(const std::chrono::duration<Rep, Period>& x)
    {
        local_time_format_info fmt;
        if(this->is_local_time())
        {
            fmt = this->as_local_time_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_time;
        this->region_ = region_type{};
        assigner(this->local_time_, local_time_storage(local_time_type(x), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (local_datetime) =========================================== {{{

    basic_value(local_datetime_type x)
        : basic_value(x, local_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_datetime_type x, std::vector<std::string> com)
        : basic_value(x, local_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_datetime), local_datetime_(local_datetime_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(local_datetime_type x)
    {
        local_datetime_format_info fmt;
        if(this->is_local_datetime())
        {
            fmt = this->as_local_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_datetime;
        this->region_ = region_type{};
        assigner(this->local_datetime_, local_datetime_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (offset_datetime) =========================================== {{{

    basic_value(offset_datetime_type x)
        : basic_value(x, offset_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(offset_datetime_type x, std::vector<std::string> com)
        : basic_value(x, offset_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::offset_datetime), offset_datetime_(offset_datetime_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(offset_datetime_type x)
    {
        offset_datetime_format_info fmt;
        if(this->is_offset_datetime())
        {
            fmt = this->as_offset_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::offset_datetime;
        this->region_ = region_type{};
        assigner(this->offset_datetime_, offset_datetime_storage(x, fmt));
        return *this;
    }

    // system_clock::time_point

    basic_value(std::chrono::system_clock::time_point x)
        : basic_value(offset_datetime_type(x), offset_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt)
        : basic_value(offset_datetime_type(x), fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, std::vector<std::string> com)
        : basic_value(offset_datetime_type(x), offset_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(offset_datetime_type(x), fmt, std::move(com), region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : basic_value(offset_datetime_type(x), std::move(fmt), std::move(com), std::move(reg))
    {}
    basic_value& operator=(std::chrono::system_clock::time_point x)
    {
        offset_datetime_format_info fmt;
        if(this->is_offset_datetime())
        {
            fmt = this->as_offset_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::offset_datetime;
        this->region_ = region_type{};
        assigner(this->offset_datetime_, offset_datetime_storage(offset_datetime_type(x), fmt));
        return *this;
    }

    // }}}

    // constructor (array) ================================================ {{{

    basic_value(array_type x)
        : basic_value(std::move(x), array_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(array_type x, array_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(array_type x, std::vector<std::string> com)
        : basic_value(std::move(x), array_format_info{}, std::move(com), region_type{})
    {}
    basic_value(array_type x, array_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    basic_value(array_type x, array_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::array), array_(array_storage(
              detail::storage<array_type>(std::move(x)), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(array_type x)
    {
        array_format_info fmt;
        if(this->is_array())
        {
            fmt = this->as_array_fmt();
        }
        this->cleanup();
        this->type_   = value_t::array;
        this->region_ = region_type{};
        assigner(this->array_, array_storage(
                    detail::storage<array_type>(std::move(x)), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_array_like_t = cxx::enable_if_t<cxx::conjunction<
            detail::is_container<T>,
            cxx::negation<std::is_same<T, array_type>>,
            cxx::negation<detail::is_std_basic_string<T>>,
#if defined(TOML11_HAS_STRING_VIEW)
            cxx::negation<detail::is_std_basic_string_view<T>>,
#endif
            cxx::negation<detail::has_from_toml_method<T, config_type>>,
            cxx::negation<detail::has_specialized_from<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), array_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), array_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::array), array_(array_storage(
              detail::storage<array_type>(array_type(
                      std::make_move_iterator(x.begin()),
                      std::make_move_iterator(x.end()))
              ), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        array_format_info fmt;
        if(this->is_array())
        {
            fmt = this->as_array_fmt();
        }
        this->cleanup();
        this->type_   = value_t::array;
        this->region_ = region_type{};

        array_type a(std::make_move_iterator(x.begin()),
                     std::make_move_iterator(x.end()));
        assigner(this->array_, array_storage(
                    detail::storage<array_type>(std::move(a)), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (table) ================================================ {{{

    basic_value(table_type x)
        : basic_value(std::move(x), table_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(table_type x, table_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(table_type x, std::vector<std::string> com)
        : basic_value(std::move(x), table_format_info{}, std::move(com), region_type{})
    {}
    basic_value(table_type x, table_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    basic_value(table_type x, table_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::table), table_(table_storage(
                detail::storage<table_type>(std::move(x)), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
    {}
    basic_value& operator=(table_type x)
    {
        table_format_info fmt;
        if(this->is_table())
        {
            fmt = this->as_table_fmt();
        }
        this->cleanup();
        this->type_   = value_t::table;
        this->region_ = region_type{};
        assigner(this->table_, table_storage(
            detail::storage<table_type>(std::move(x)), std::move(fmt)));
        return *this;
    }

    // table-like

  private:

    template<typename T>
    using enable_if_table_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<T, table_type>>,
            detail::is_map<T>,
            cxx::negation<detail::has_from_toml_method<T, config_type>>,
            cxx::negation<detail::has_specialized_from<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), table_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), table_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::table), table_(table_storage(
              detail::storage<table_type>(table_type(
                      std::make_move_iterator(x.begin()),
                      std::make_move_iterator(x.end())
              )), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        table_format_info fmt;
        if(this->is_table())
        {
            fmt = this->as_table_fmt();
        }
        this->cleanup();
        this->type_   = value_t::table;
        this->region_ = region_type{};

        table_type t(std::make_move_iterator(x.begin()),
                     std::make_move_iterator(x.end()));
        assigner(this->table_, table_storage(
            detail::storage<table_type>(std::move(t)), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (user_defined) ========================================= {{{

    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud)
        : basic_value(
            into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud))
    {}
    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(
            into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud),
            std::move(com))
    {}
    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud);
        return *this;
    }

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud): basic_value(ud.into_toml()) {}

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(ud.into_toml(), std::move(com))
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = ud.into_toml();
        return *this;
    }

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud): basic_value(ud.template into_toml<TypeConfig>()) {}

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(ud.template into_toml<TypeConfig>(), std::move(com))
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = ud.template into_toml<TypeConfig>();
        return *this;
    }
    // }}}

    // empty value with region info ======================================= {{{

    // mainly for `null` extension
    basic_value(detail::none_t, region_type reg) noexcept
        : type_(value_t::empty), empty_('\0'), region_(std::move(reg)), comments_{}
    {}

    // }}}

    // type checking ====================================================== {{{

    template<typename T, cxx::enable_if_t<
        detail::is_exact_toml_type<cxx::remove_cvref_t<T>, value_type>::value,
        std::nullptr_t> = nullptr>
    bool is() const noexcept
    {
        return detail::type_to_enum<T, value_type>::value == this->type_;
    }
    bool is(value_t t) const noexcept {return t == this->type_;}

    bool is_empty()           const noexcept {return this->is(value_t::empty          );}
    bool is_boolean()         const noexcept {return this->is(value_t::boolean        );}
    bool is_integer()         const noexcept {return this->is(value_t::integer        );}
    bool is_floating()        const noexcept {return this->is(value_t::floating       );}
    bool is_string()          const noexcept {return this->is(value_t::string         );}
    bool is_offset_datetime() const noexcept {return this->is(value_t::offset_datetime);}
    bool is_local_datetime()  const noexcept {return this->is(value_t::local_datetime );}
    bool is_local_date()      const noexcept {return this->is(value_t::local_date     );}
    bool is_local_time()      const noexcept {return this->is(value_t::local_time     );}
    bool is_array()           const noexcept {return this->is(value_t::array          );}
    bool is_table()           const noexcept {return this->is(value_t::table          );}

    bool is_array_of_tables() const noexcept
    {
        if( ! this->is_array()) {return false;}
        const auto& a = this->as_array(std::nothrow); // already checked.

        // when you define [[array.of.tables]], at least one empty table will be
        // assigned. In case of array of inline tables, `array_of_tables = []`,
        // there is no reason to consider this as an array of *tables*.
        // So empty array is not an array-of-tables.
        if(a.empty()) {return false;}

        // since toml v1.0.0 allows array of heterogeneous types, we need to
        // check all the elements. if any of the elements is not a table, it
        // is a heterogeneous array and cannot be expressed by `[[aot]]` form.
        for(const auto& e : a)
        {
            if( ! e.is_table()) {return false;}
        }
        return true;
    }

    value_t type() const noexcept {return type_;}

    // }}}

    // as_xxx (noexcept) version ========================================== {{{

    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>> const&
    as(const std::nothrow_t&) const noexcept
    {
        return detail::getter<config_type, T>::get_nothrow(*this);
    }
    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>>&
    as(const std::nothrow_t&) noexcept
    {
        return detail::getter<config_type, T>::get_nothrow(*this);
    }

    boolean_type         const& as_boolean        (const std::nothrow_t&) const noexcept {return this->boolean_.value;}
    integer_type         const& as_integer        (const std::nothrow_t&) const noexcept {return this->integer_.value;}
    floating_type        const& as_floating       (const std::nothrow_t&) const noexcept {return this->floating_.value;}
    string_type          const& as_string         (const std::nothrow_t&) const noexcept {return this->string_.value;}
    offset_datetime_type const& as_offset_datetime(const std::nothrow_t&) const noexcept {return this->offset_datetime_.value;}
    local_datetime_type  const& as_local_datetime (const std::nothrow_t&) const noexcept {return this->local_datetime_.value;}
    local_date_type      const& as_local_date     (const std::nothrow_t&) const noexcept {return this->local_date_.value;}
    local_time_type      const& as_local_time     (const std::nothrow_t&) const noexcept {return this->local_time_.value;}
    array_type           const& as_array          (const std::nothrow_t&) const noexcept {return this->array_.value.get();}
    table_type           const& as_table          (const std::nothrow_t&) const noexcept {return this->table_.value.get();}

    boolean_type        & as_boolean        (const std::nothrow_t&) noexcept {return this->boolean_.value;}
    integer_type        & as_integer        (const std::nothrow_t&) noexcept {return this->integer_.value;}
    floating_type       & as_floating       (const std::nothrow_t&) noexcept {return this->floating_.value;}
    string_type         & as_string         (const std::nothrow_t&) noexcept {return this->string_.value;}
    offset_datetime_type& as_offset_datetime(const std::nothrow_t&) noexcept {return this->offset_datetime_.value;}
    local_datetime_type & as_local_datetime (const std::nothrow_t&) noexcept {return this->local_datetime_.value;}
    local_date_type     & as_local_date     (const std::nothrow_t&) noexcept {return this->local_date_.value;}
    local_time_type     & as_local_time     (const std::nothrow_t&) noexcept {return this->local_time_.value;}
    array_type          & as_array          (const std::nothrow_t&) noexcept {return this->array_.value.get();}
    table_type          & as_table          (const std::nothrow_t&) noexcept {return this->table_.value.get();}

    // }}}

    // as_xxx (throw) ===================================================== {{{

    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>> const& as() const
    {
        return detail::getter<config_type, T>::get(*this);
    }
    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>>& as()
    {
        return detail::getter<config_type, T>::get(*this);
    }

    boolean_type const& as_boolean() const
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean()", value_t::boolean);
        }
        return this->boolean_.value;
    }
    integer_type const& as_integer() const
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer()", value_t::integer);
        }
        return this->integer_.value;
    }
    floating_type const& as_floating() const
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating()", value_t::floating);
        }
        return this->floating_.value;
    }
    string_type const& as_string() const
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string()", value_t::string);
        }
        return this->string_.value;
    }
    offset_datetime_type const& as_offset_datetime() const
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime()", value_t::offset_datetime);
        }
        return this->offset_datetime_.value;
    }
    local_datetime_type const& as_local_datetime() const
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime()", value_t::local_datetime);
        }
        return this->local_datetime_.value;
    }
    local_date_type const& as_local_date() const
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date()", value_t::local_date);
        }
        return this->local_date_.value;
    }
    local_time_type const& as_local_time() const
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time()", value_t::local_time);
        }
        return this->local_time_.value;
    }
    array_type const& as_array() const
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array()", value_t::array);
        }
        return this->array_.value.get();
    }
    table_type const& as_table() const
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table()", value_t::table);
        }
        return this->table_.value.get();
    }

    // ------------------------------------------------------------------------
    // nonconst reference

    boolean_type& as_boolean()
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean()", value_t::boolean);
        }
        return this->boolean_.value;
    }
    integer_type& as_integer()
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer()", value_t::integer);
        }
        return this->integer_.value;
    }
    floating_type& as_floating()
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating()", value_t::floating);
        }
        return this->floating_.value;
    }
    string_type& as_string()
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string()", value_t::string);
        }
        return this->string_.value;
    }
    offset_datetime_type& as_offset_datetime()
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime()", value_t::offset_datetime);
        }
        return this->offset_datetime_.value;
    }
    local_datetime_type& as_local_datetime()
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime()", value_t::local_datetime);
        }
        return this->local_datetime_.value;
    }
    local_date_type& as_local_date()
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date()", value_t::local_date);
        }
        return this->local_date_.value;
    }
    local_time_type& as_local_time()
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time()", value_t::local_time);
        }
        return this->local_time_.value;
    }
    array_type& as_array()
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array()", value_t::array);
        }
        return this->array_.value.get();
    }
    table_type& as_table()
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table()", value_t::table);
        }
        return this->table_.value.get();
    }

    // }}}

    // format accessors (noexcept) ======================================== {{{

    template<value_t T>
    detail::enum_to_fmt_type_t<T> const&
    as_fmt(const std::nothrow_t&) const noexcept
    {
        return detail::getter<config_type, T>::get_fmt_nothrow(*this);
    }
    template<value_t T>
    detail::enum_to_fmt_type_t<T>&
    as_fmt(const std::nothrow_t&) noexcept
    {
        return detail::getter<config_type, T>::get_fmt_nothrow(*this);
    }

    boolean_format_info        & as_boolean_fmt        (const std::nothrow_t&) noexcept {return this->boolean_.format;}
    integer_format_info        & as_integer_fmt        (const std::nothrow_t&) noexcept {return this->integer_.format;}
    floating_format_info       & as_floating_fmt       (const std::nothrow_t&) noexcept {return this->floating_.format;}
    string_format_info         & as_string_fmt         (const std::nothrow_t&) noexcept {return this->string_.format;}
    offset_datetime_format_info& as_offset_datetime_fmt(const std::nothrow_t&) noexcept {return this->offset_datetime_.format;}
    local_datetime_format_info & as_local_datetime_fmt (const std::nothrow_t&) noexcept {return this->local_datetime_.format;}
    local_date_format_info     & as_local_date_fmt     (const std::nothrow_t&) noexcept {return this->local_date_.format;}
    local_time_format_info     & as_local_time_fmt     (const std::nothrow_t&) noexcept {return this->local_time_.format;}
    array_format_info          & as_array_fmt          (const std::nothrow_t&) noexcept {return this->array_.format;}
    table_format_info          & as_table_fmt          (const std::nothrow_t&) noexcept {return this->table_.format;}

    boolean_format_info         const& as_boolean_fmt        (const std::nothrow_t&) const noexcept {return this->boolean_.format;}
    integer_format_info         const& as_integer_fmt        (const std::nothrow_t&) const noexcept {return this->integer_.format;}
    floating_format_info        const& as_floating_fmt       (const std::nothrow_t&) const noexcept {return this->floating_.format;}
    string_format_info          const& as_string_fmt         (const std::nothrow_t&) const noexcept {return this->string_.format;}
    offset_datetime_format_info const& as_offset_datetime_fmt(const std::nothrow_t&) const noexcept {return this->offset_datetime_.format;}
    local_datetime_format_info  const& as_local_datetime_fmt (const std::nothrow_t&) const noexcept {return this->local_datetime_.format;}
    local_date_format_info      const& as_local_date_fmt     (const std::nothrow_t&) const noexcept {return this->local_date_.format;}
    local_time_format_info      const& as_local_time_fmt     (const std::nothrow_t&) const noexcept {return this->local_time_.format;}
    array_format_info           const& as_array_fmt          (const std::nothrow_t&) const noexcept {return this->array_.format;}
    table_format_info           const& as_table_fmt          (const std::nothrow_t&) const noexcept {return this->table_.format;}

    // }}}

    // format accessors (throw) =========================================== {{{

    template<value_t T>
    detail::enum_to_fmt_type_t<T> const& as_fmt() const
    {
        return detail::getter<config_type, T>::get_fmt(*this);
    }
    template<value_t T>
    detail::enum_to_fmt_type_t<T>& as_fmt()
    {
        return detail::getter<config_type, T>::get_fmt(*this);
    }

    boolean_format_info const& as_boolean_fmt() const
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean_fmt()", value_t::boolean);
        }
        return this->boolean_.format;
    }
    integer_format_info const& as_integer_fmt() const
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer_fmt()", value_t::integer);
        }
        return this->integer_.format;
    }
    floating_format_info const& as_floating_fmt() const
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating_fmt()", value_t::floating);
        }
        return this->floating_.format;
    }
    string_format_info const& as_string_fmt() const
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string_fmt()", value_t::string);
        }
        return this->string_.format;
    }
    offset_datetime_format_info const& as_offset_datetime_fmt() const
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime_fmt()", value_t::offset_datetime);
        }
        return this->offset_datetime_.format;
    }
    local_datetime_format_info const& as_local_datetime_fmt() const
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime_fmt()", value_t::local_datetime);
        }
        return this->local_datetime_.format;
    }
    local_date_format_info const& as_local_date_fmt() const
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date_fmt()", value_t::local_date);
        }
        return this->local_date_.format;
    }
    local_time_format_info const& as_local_time_fmt() const
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time_fmt()", value_t::local_time);
        }
        return this->local_time_.format;
    }
    array_format_info const& as_array_fmt() const
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array_fmt()", value_t::array);
        }
        return this->array_.format;
    }
    table_format_info const& as_table_fmt() const
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table_fmt()", value_t::table);
        }
        return this->table_.format;
    }

    // ------------------------------------------------------------------------
    // nonconst reference

    boolean_format_info& as_boolean_fmt()
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean_fmt()", value_t::boolean);
        }
        return this->boolean_.format;
    }
    integer_format_info& as_integer_fmt()
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer_fmt()", value_t::integer);
        }
        return this->integer_.format;
    }
    floating_format_info& as_floating_fmt()
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating_fmt()", value_t::floating);
        }
        return this->floating_.format;
    }
    string_format_info& as_string_fmt()
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string_fmt()", value_t::string);
        }
        return this->string_.format;
    }
    offset_datetime_format_info& as_offset_datetime_fmt()
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime_fmt()", value_t::offset_datetime);
        }
        return this->offset_datetime_.format;
    }
    local_datetime_format_info& as_local_datetime_fmt()
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime_fmt()", value_t::local_datetime);
        }
        return this->local_datetime_.format;
    }
    local_date_format_info& as_local_date_fmt()
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date_fmt()", value_t::local_date);
        }
        return this->local_date_.format;
    }
    local_time_format_info& as_local_time_fmt()
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time_fmt()", value_t::local_time);
        }
        return this->local_time_.format;
    }
    array_format_info& as_array_fmt()
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array_fmt()", value_t::array);
        }
        return this->array_.format;
    }
    table_format_info& as_table_fmt()
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table_fmt()", value_t::table);
        }
        return this->table_.format;
    }
    // }}}

    // table accessors ==================================================== {{{

    value_type& at(const key_type& k)
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::at(key_type)", value_t::table);
        }
        auto& table = this->as_table(std::nothrow);
        const auto found = table.find(k);
        if(found == table.end())
        {
            this->throw_key_not_found_error("toml::value::at", k);
        }
        assert(found->first == k);
        return found->second;
    }
    value_type const& at(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::at(key_type)", value_t::table);
        }
        const auto& table = this->as_table(std::nothrow);
        const auto found = table.find(k);
        if(found == table.end())
        {
            this->throw_key_not_found_error("toml::value::at", k);
        }
        assert(found->first == k);
        return found->second;
    }
    value_type& operator[](const key_type& k)
    {
        if(this->is_empty())
        {
            (*this) = table_type{};
        }
        else if( ! this->is_table()) // initialized, but not a table
        {
            this->throw_bad_cast("toml::value::operator[](key_type)", value_t::table);
        }
        return (this->as_table(std::nothrow))[k];
    }
    std::size_t count(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::count(key_type)", value_t::table);
        }
        return this->as_table(std::nothrow).count(k);
    }
    bool contains(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::contains(key_type)", value_t::table);
        }
        const auto& table = this->as_table(std::nothrow);
        return table.find(k) != table.end();
    }
    // }}}

    // array accessors ==================================================== {{{

    value_type& at(const std::size_t idx)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::at(idx)", value_t::array);
        }
        auto& ar = this->as_array(std::nothrow);

        if(ar.size() <= idx)
        {
            std::ostringstream oss;
            oss << "actual length (" << ar.size()
                << ") is shorter than the specified index (" << idx << ").";
            throw std::out_of_range(format_error(
                "toml::value::at(idx): no element corresponding to the index",
                this->location(), oss.str()
                ));
        }
        return ar.at(idx);
    }
    value_type const& at(const std::size_t idx) const
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::at(idx)", value_t::array);
        }
        const auto& ar = this->as_array(std::nothrow);

        if(ar.size() <= idx)
        {
            std::ostringstream oss;
            oss << "actual length (" << ar.size()
                << ") is shorter than the specified index (" << idx << ").";

            throw std::out_of_range(format_error(
                "toml::value::at(idx): no element corresponding to the index",
                this->location(), oss.str()
                ));
        }
        return ar.at(idx);
    }

    value_type&       operator[](const std::size_t idx) noexcept
    {
        // no check...
        return this->as_array(std::nothrow)[idx];
    }
    value_type const& operator[](const std::size_t idx) const noexcept
    {
        // no check...
        return this->as_array(std::nothrow)[idx];
    }

    void push_back(const value_type& x)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::push_back(idx)", value_t::array);
        }
        this->as_array(std::nothrow).push_back(x);
        return;
    }
    void push_back(value_type&& x)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::push_back(idx)", value_t::array);
        }
        this->as_array(std::nothrow).push_back(std::move(x));
        return;
    }

    template<typename ... Ts>
    value_type& emplace_back(Ts&& ... args)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::emplace_back(idx)", value_t::array);
        }
        auto& ar = this->as_array(std::nothrow);
        ar.emplace_back(std::forward<Ts>(args) ...);
        return ar.back();
    }

    std::size_t size() const
    {
        switch(this->type_)
        {
            case value_t::array:
            {
                return this->as_array(std::nothrow).size();
            }
            case value_t::table:
            {
                return this->as_table(std::nothrow).size();
            }
            case value_t::string:
            {
                return this->as_string(std::nothrow).size();
            }
            default:
            {
                throw type_error(format_error(
                    "toml::value::size(): bad_cast to container types",
                    this->location(),
                    "the actual type is " + to_string(this->type_)
                    ), this->location());
            }
        }
    }

    // }}}

    source_location location() const
    {
        return source_location(this->region_);
    }

    comment_type const& comments() const noexcept {return this->comments_;}
    comment_type&       comments()       noexcept {return this->comments_;}

  private:

    // private helper functions =========================================== {{{

    void cleanup() noexcept
    {
        switch(this->type_)
        {
            case value_t::boolean         : { boolean_        .~boolean_storage         (); break; }
            case value_t::integer         : { integer_        .~integer_storage         (); break; }
            case value_t::floating        : { floating_       .~floating_storage        (); break; }
            case value_t::string          : { string_         .~string_storage          (); break; }
            case value_t::offset_datetime : { offset_datetime_.~offset_datetime_storage (); break; }
            case value_t::local_datetime  : { local_datetime_ .~local_datetime_storage  (); break; }
            case value_t::local_date      : { local_date_     .~local_date_storage      (); break; }
            case value_t::local_time      : { local_time_     .~local_time_storage      (); break; }
            case value_t::array           : { array_          .~array_storage           (); break; }
            case value_t::table           : { table_          .~table_storage           (); break; }
            default                       : { break; }
        }
        this->type_ = value_t::empty;
        return;
    }

    template<typename T, typename U>
    static void assigner(T& dst, U&& v)
    {
        const auto tmp = ::new(std::addressof(dst)) T(std::forward<U>(v));
        assert(tmp == std::addressof(dst));
        (void)tmp;
    }

    [[noreturn]]
    void throw_bad_cast(const std::string& funcname, const value_t ty) const
    {
        throw type_error(format_error(detail::make_type_error(*this, funcname, ty)),
                         this->location());
    }

    [[noreturn]]
    void throw_key_not_found_error(const std::string& funcname, const key_type& key) const
    {
        throw std::out_of_range(format_error(
                    detail::make_not_found_error(*this, funcname, key)));
    }

    template<typename TC>
    friend void detail::change_region_of_value(basic_value<TC>&, const basic_value<TC>&);

    template<typename TC>
    friend class basic_value;

    // }}}

  private:

    using boolean_storage         = detail::value_with_format<boolean_type,                boolean_format_info        >;
    using integer_storage         = detail::value_with_format<integer_type,                integer_format_info        >;
    using floating_storage        = detail::value_with_format<floating_type,               floating_format_info       >;
    using string_storage          = detail::value_with_format<string_type,                 string_format_info         >;
    using offset_datetime_storage = detail::value_with_format<offset_datetime_type,        offset_datetime_format_info>;
    using local_datetime_storage  = detail::value_with_format<local_datetime_type,         local_datetime_format_info >;
    using local_date_storage      = detail::value_with_format<local_date_type,             local_date_format_info     >;
    using local_time_storage      = detail::value_with_format<local_time_type,             local_time_format_info     >;
    using array_storage           = detail::value_with_format<detail::storage<array_type>, array_format_info          >;
    using table_storage           = detail::value_with_format<detail::storage<table_type>, table_format_info          >;

  private:

    value_t type_;
    union
    {
        char                    empty_; // the smallest type
        boolean_storage         boolean_;
        integer_storage         integer_;
        floating_storage        floating_;
        string_storage          string_;
        offset_datetime_storage offset_datetime_;
        local_datetime_storage  local_datetime_;
        local_date_storage      local_date_;
        local_time_storage      local_time_;
        array_storage           array_;
        table_storage           table_;
    };
    region_type  region_;
    comment_type comments_;
};

template<typename TC>
bool operator==(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    if(lhs.type()     != rhs.type())     {return false;}
    if(lhs.comments() != rhs.comments()) {return false;}

    switch(lhs.type())
    {
        case value_t::boolean  :
        {
            return lhs.as_boolean() == rhs.as_boolean();
        }
        case value_t::integer  :
        {
            return lhs.as_integer() == rhs.as_integer();
        }
        case value_t::floating :
        {
            return lhs.as_floating() == rhs.as_floating();
        }
        case value_t::string   :
        {
            return lhs.as_string() == rhs.as_string();
        }
        case value_t::offset_datetime:
        {
            return lhs.as_offset_datetime() == rhs.as_offset_datetime();
        }
        case value_t::local_datetime:
        {
            return lhs.as_local_datetime() == rhs.as_local_datetime();
        }
        case value_t::local_date:
        {
            return lhs.as_local_date() == rhs.as_local_date();
        }
        case value_t::local_time:
        {
            return lhs.as_local_time() == rhs.as_local_time();
        }
        case value_t::array    :
        {
            return lhs.as_array() == rhs.as_array();
        }
        case value_t::table    :
        {
            return lhs.as_table() == rhs.as_table();
        }
        case value_t::empty    : {return true; }
        default:                 {return false;}
    }
}

template<typename TC>
bool operator!=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs == rhs);
}

template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator<(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    if(lhs.type() != rhs.type())
    {
        return (lhs.type() < rhs.type());
    }
    switch(lhs.type())
    {
        case value_t::boolean  :
        {
            return lhs.as_boolean() <  rhs.as_boolean() ||
                  (lhs.as_boolean() == rhs.as_boolean() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::integer  :
        {
            return lhs.as_integer() <  rhs.as_integer() ||
                  (lhs.as_integer() == rhs.as_integer() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::floating :
        {
            return lhs.as_floating() <  rhs.as_floating() ||
                  (lhs.as_floating() == rhs.as_floating() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::string   :
        {
            return lhs.as_string() <  rhs.as_string() ||
                  (lhs.as_string() == rhs.as_string() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::offset_datetime:
        {
            return lhs.as_offset_datetime() <  rhs.as_offset_datetime() ||
                  (lhs.as_offset_datetime() == rhs.as_offset_datetime() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_datetime:
        {
            return lhs.as_local_datetime() <  rhs.as_local_datetime() ||
                  (lhs.as_local_datetime() == rhs.as_local_datetime() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_date:
        {
            return lhs.as_local_date() <  rhs.as_local_date() ||
                  (lhs.as_local_date() == rhs.as_local_date() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_time:
        {
            return lhs.as_local_time() <  rhs.as_local_time() ||
                  (lhs.as_local_time() == rhs.as_local_time() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::array    :
        {
            return lhs.as_array() <  rhs.as_array() ||
                  (lhs.as_array() == rhs.as_array() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::table    :
        {
            return lhs.as_table() <  rhs.as_table() ||
                  (lhs.as_table() == rhs.as_table() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::empty    :
        {
            return lhs.comments() < rhs.comments();
        }
        default:
        {
            return lhs.comments() < rhs.comments();
        }
    }
}

template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator<=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator>(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs <= rhs);
}
template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator>=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs < rhs);
}

// error_info helper
namespace detail
{
template<typename TC, typename ... Ts>
error_info make_error_info_rec(error_info e,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return make_error_info_rec(std::move(e), v.location(), std::move(msg), std::forward<Ts>(tail)...);
}
} // detail

template<typename TC, typename ... Ts>
error_info make_error_info(
        std::string title, const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return make_error_info(std::move(title),
            v.location(), std::move(msg), std::forward<Ts>(tail)...);
}
template<typename TC, typename ... Ts>
std::string format_error(std::string title,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return format_error(std::move(title),
            v.location(), std::move(msg), std::forward<Ts>(tail)...);
}

namespace detail
{

template<typename TC>
error_info make_type_error(const basic_value<TC>& v, const std::string& fname, const value_t ty)
{
    return make_error_info(fname + ": bad_cast to " + to_string(ty),
        v.location(), "the actual type is " + to_string(v.type()));
}
template<typename TC>
error_info make_not_found_error(const basic_value<TC>& v, const std::string& fname, const typename basic_value<TC>::key_type& key)
{
    const auto loc = v.location();
    const std::string title = fname + ": key \"" + string_conv<std::string>(key) + "\" not found";

    std::vector<std::pair<source_location, std::string>> locs;
    if( ! loc.is_ok())
    {
        return error_info(title, locs);
    }

    if(loc.first_line_number() == 1 && loc.first_column_number() == 1 && loc.length() == 1)
    {
        // The top-level table has its region at the 0th character of the file.
        // That means that, in the case when a key is not found in the top-level
        // table, the error message points to the first character. If the file has
        // the first table at the first line, the error message would be like this.
        // ```console
        // [error] key "a" not found
        //  --> example.toml
        //    |
        //  1 | [table]
        //    | ^------ in this table
        // ```
        // It actually points to the top-level table at the first character, not
        // `[table]`. But it is too confusing. To avoid the confusion, the error
        // message should explicitly say "key not found in the top-level table".
        locs.emplace_back(v.location(), "at the top-level table");
    }
    else
    {
        locs.emplace_back(v.location(), "in this table");
    }
    return error_info(title, locs);
}

#define TOML11_DETAIL_GENERATE_COMPTIME_GETTER(ty)                              \
    template<typename TC>                                                       \
    struct getter<TC, value_t::ty>                                              \
    {                                                                           \
        using value_type = basic_value<TC>;                                     \
        using result_type = enum_to_type_t<value_t::ty, value_type>;            \
        using format_type = enum_to_fmt_type_t<value_t::ty>;                    \
                                                                                \
        static result_type&       get(value_type& v)                            \
        {                                                                       \
            return v.as_ ## ty();                                               \
        }                                                                       \
        static result_type const& get(const value_type& v)                      \
        {                                                                       \
            return v.as_ ## ty();                                               \
        }                                                                       \
                                                                                \
        static result_type&       get_nothrow(value_type& v) noexcept           \
        {                                                                       \
            return v.as_ ## ty(std::nothrow);                                   \
        }                                                                       \
        static result_type const& get_nothrow(const value_type& v) noexcept     \
        {                                                                       \
            return v.as_ ## ty(std::nothrow);                                   \
        }                                                                       \
                                                                                \
        static format_type&       get_fmt(value_type& v)                        \
        {                                                                       \
            return v.as_ ## ty ## _fmt();                                       \
        }                                                                       \
        static format_type const& get_fmt(const value_type& v)                  \
        {                                                                       \
            return v.as_ ## ty ## _fmt();                                       \
        }                                                                       \
                                                                                \
        static format_type&       get_fmt_nothrow(value_type& v) noexcept       \
        {                                                                       \
            return v.as_ ## ty ## _fmt(std::nothrow);                           \
        }                                                                       \
        static format_type const& get_fmt_nothrow(const value_type& v) noexcept \
        {                                                                       \
            return v.as_ ## ty ## _fmt(std::nothrow);                           \
        }                                                                       \
    };

TOML11_DETAIL_GENERATE_COMPTIME_GETTER(boolean        )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(integer        )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(floating       )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(string         )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(offset_datetime)
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_datetime )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_date     )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_time     )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(array          )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(table          )

#undef TOML11_DETAIL_GENERATE_COMPTIME_GETTER

template<typename TC>
void change_region_of_value(basic_value<TC>& dst, const basic_value<TC>& src)
{
    dst.region_ = std::move(src.region_);
    return;
}

} // namespace detail
} // namespace toml
#endif // TOML11_VALUE_HPP
