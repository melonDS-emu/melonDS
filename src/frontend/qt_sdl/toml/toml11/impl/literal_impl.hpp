#ifndef TOML11_LITERAL_IMPL_HPP
#define TOML11_LITERAL_IMPL_HPP

#include "../fwd/literal_fwd.hpp"
#include "../parser.hpp"
#include "../syntax.hpp"

namespace toml
{

namespace detail
{
// implementation
TOML11_INLINE ::toml::value literal_internal_impl(location loc)
{
    const auto s = ::toml::spec::default_version();
    context<type_config> ctx(s);

    const auto front = loc;

    // ------------------------------------------------------------------------
    // check if it is a raw value.

    // skip empty lines and comment lines
    auto sp = skip_multiline_spacer(loc, ctx);
    if(loc.eof())
    {
        ::toml::value val;
        if(sp.has_value())
        {
            for(std::size_t i=0; i<sp.value().comments.size(); ++i)
            {
                val.comments().push_back(std::move(sp.value().comments.at(i)));
            }
        }
        return val;
    }

    // to distinguish arrays and tables, first check it is a table or not.
    //
    // "[1,2,3]"_toml;   // json: [1, 2, 3]
    // "[table]"_toml;   // json: {"table": {}}
    // "[[1,2,3]]"_toml; // json: [[1, 2, 3]]
    // "[[table]]"_toml; // json: {"table": [{}]}
    //
    // "[[1]]"_toml;     // json: {"1": [{}]}
    // "1 = [{}]"_toml;  // json: {"1": [{}]}
    // "[[1,]]"_toml;    // json: [[1]]
    // "[[1],]"_toml;    // json: [[1]]
    const auto val_start = loc;

    const bool is_table_key = syntax::std_table(s).scan(loc).is_ok();
    loc = val_start;
    const bool is_aots_key  = syntax::array_table(s).scan(loc).is_ok();
    loc = val_start;

    // If it is neither a table-key or a array-of-table-key, it may be a value.
    if(!is_table_key && !is_aots_key)
    {
        auto data = parse_value(loc, ctx);
        if(data.is_ok())
        {
            auto val = std::move(data.unwrap());
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    val.comments().push_back(std::move(sp.value().comments.at(i)));
                }
            }
            auto com_res = parse_comment_line(loc, ctx);
            if(com_res.is_ok() && com_res.unwrap().has_value())
            {
                val.comments().push_back(com_res.unwrap().value());
            }
            return val;
        }
    }

    // -------------------------------------------------------------------------
    // Note that still it can be a table, because the literal might be something
    // like the following.
    // ```cpp
    // // c++11 raw-string literal
    // const auto val = R"(
    //   key = "value"
    //   int = 42
    // )"_toml;
    // ```
    // It is a valid toml file.
    // It should be parsed as if we parse a file with this content.

    loc = front;
    auto data = parse_file(loc, ctx);
    if(data.is_ok())
    {
        return data.unwrap();
    }
    else // not a value && not a file. error.
    {
        std::string msg;
        for(const auto& err : data.unwrap_err())
        {
            msg += format_error(err);
        }
        throw ::toml::syntax_error(std::move(msg), std::move(data.unwrap_err()));
    }
}

} // detail

inline namespace literals
{
inline namespace toml_literals
{

TOML11_INLINE ::toml::value
operator"" _toml(const char* str, std::size_t len)
{
    if(len == 0)
    {
        return ::toml::value{};
    }

    ::toml::detail::location::container_type c(len);
    std::copy(reinterpret_cast<const ::toml::detail::location::char_type*>(str),
              reinterpret_cast<const ::toml::detail::location::char_type*>(str + len),
              c.begin());
    if( ! c.empty() && c.back())
    {
        c.push_back('\n'); // to make it easy to parse comment, we add newline
    }

    return literal_internal_impl(::toml::detail::location(
        std::make_shared<const toml::detail::location::container_type>(std::move(c)),
        "TOML literal encoded in a C++ code"));
}

#if defined(__cpp_char8_t)
#  if __cpp_char8_t >= 201811L
#    define TOML11_HAS_CHAR8_T 1
#  endif
#endif

#if defined(TOML11_HAS_CHAR8_T)
// value of u8"" literal has been changed from char to char8_t and char8_t is
// NOT compatible to char
TOML11_INLINE ::toml::value
operator"" _toml(const char8_t* str, std::size_t len)
{
    if(len == 0)
    {
        return ::toml::value{};
    }

    ::toml::detail::location::container_type c(len);
    std::copy(reinterpret_cast<const ::toml::detail::location::char_type*>(str),
              reinterpret_cast<const ::toml::detail::location::char_type*>(str + len),
              c.begin());
    if( ! c.empty() && c.back())
    {
        c.push_back('\n'); // to make it easy to parse comment, we add newline
    }

    return literal_internal_impl(::toml::detail::location(
        std::make_shared<const toml::detail::location::container_type>(std::move(c)),
        "TOML literal encoded in a C++ code"));
}
#endif

} // toml_literals
} // literals
} // toml
#endif // TOML11_LITERAL_IMPL_HPP
