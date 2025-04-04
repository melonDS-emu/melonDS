#ifndef TOML11_PARSER_HPP
#define TOML11_PARSER_HPP

#include "context.hpp"
#include "datetime.hpp"
#include "error_info.hpp"
#include "region.hpp"
#include "result.hpp"
#include "scanner.hpp"
#include "skip.hpp"
#include "syntax.hpp"
#include "value.hpp"

#include <fstream>
#include <sstream>

#include <cassert>
#include <cmath>

#if defined(TOML11_HAS_FILESYSTEM) && TOML11_HAS_FILESYSTEM
#include <filesystem>
#endif

namespace toml
{

struct syntax_error final : public ::toml::exception
{
  public:
    syntax_error(std::string what_arg, std::vector<error_info> err)
        : what_(std::move(what_arg)), err_(std::move(err))
    {}
    ~syntax_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    std::vector<error_info> const& errors() const noexcept
    {
        return err_;
    }

  private:
    std::string what_;
    std::vector<error_info> err_;
};

struct file_io_error final : public ::toml::exception
{
  public:

    file_io_error(const std::string& msg, const std::string& fname)
        : errno_(cxx::make_nullopt()),
          what_(msg + " \"" + fname + "\"")
    {}
    file_io_error(int errnum, const std::string& msg, const std::string& fname)
        : errno_(errnum),
          what_(msg + " \"" + fname + "\": errno=" + std::to_string(errnum))
    {}
    ~file_io_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    bool has_errno() const noexcept {return errno_.has_value();}
    int  get_errno() const noexcept {return errno_.value_or(0);}

  private:

    cxx::optional<int> errno_;
    std::string what_;
};

namespace detail
{

/* ============================================================================
 *    __ ___ _ __  _ __  ___ _ _
 *   / _/ _ \ '  \| '  \/ _ \ ' \
 *   \__\___/_|_|_|_|_|_\___/_||_|
 */

template<typename S>
error_info make_syntax_error(std::string title,
        const S& scanner, location loc, std::string suffix = "")
{
    auto msg = std::string("expected ") + scanner.expected_chars(loc);
    auto src = source_location(region(loc));
    return make_error_info(
        std::move(title), std::move(src), std::move(msg), std::move(suffix));
}


/* ============================================================================
 *                             _
 *  __ ___ _ __  _ __  ___ _ _| |_
 * / _/ _ \ '  \| '  \/ -_) ' \  _|
 * \__\___/_|_|_|_|_|_\___|_||_\__|
 */

template<typename TC>
result<cxx::optional<std::string>, error_info>
parse_comment_line(location& loc, context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    const auto first = loc;

    skip_whitespace(loc, ctx);

    const auto com_reg = syntax::comment(spec).scan(loc);
    if(com_reg.is_ok())
    {
        // once comment started, newline must follow (or reach EOF).
        if( ! loc.eof() && ! syntax::newline(spec).scan(loc).is_ok())
        {
            while( ! loc.eof()) // skip until newline to continue parsing
            {
                loc.advance();
                if(loc.current() == '\n') { /*skip LF*/ loc.advance(); break; }
            }
            return err(make_error_info("toml::parse_comment_line: "
                "newline (LF / CRLF) or EOF is expected",
                source_location(region(loc)), "but got this",
                "Hint: most of the control characters are not allowed in comments"));
        }
        return ok(cxx::optional<std::string>(com_reg.as_string()));
    }
    else
    {
        loc = first; // rollback whitespace to parse indent
        return ok(cxx::optional<std::string>(cxx::make_nullopt()));
    }
}

/* ============================================================================
 *  ___           _
 * | _ ) ___  ___| |___ __ _ _ _
 * | _ \/ _ \/ _ \ / -_) _` | ' \
 * |___/\___/\___/_\___\__,_|_||_|
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_boolean(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::boolean(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_boolean: "
            "invalid boolean: boolean must be `true` or `false`, in lowercase. "
            "string must be surrounded by `\"`", syntax::boolean(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();
    const auto val = [&str]() {
        if(str == "true")
        {
            return true;
        }
        else
        {
            assert(str == "false");
            return false;
        }
    }();

    // ----------------------------------------------------------------------
    // no format info for boolean
    boolean_format_info fmt;

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *  ___     _
 * |_ _|_ _| |_ ___ __ _ ___ _ _
 *  | || ' \  _/ -_) _` / -_) '_|
 * |___|_||_\__\___\__, \___|_|
 *                 |___/
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_bin_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::bin_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_bin_integer: "
            "invalid integer: bin_int must be like: 0b0101, 0b1111_0000",
            syntax::bin_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::bin;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0b` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find(std::next(str.begin(), 2), str.end(), '1'));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0b0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    const auto val = TC::parse_int(str, source_location(region(loc)), 2);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

// ----------------------------------------------------------------------------

template<typename TC>
result<basic_value<TC>, error_info>
parse_oct_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::oct_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_oct_integer: "
            "invalid integer: oct_int must be like: 0o775, 0o04_44",
            syntax::oct_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::oct;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0o` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find_if(
                std::next(str.begin(), 2), str.end(), [](const char c) {
                    return c != '0' && c != '_';
                }));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0o0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    const auto val = TC::parse_int(str, source_location(region(loc)), 8);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_hex_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::hex_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_hex_integer: "
            "invalid integer: hex_int must be like: 0xC0FFEE, 0xdead_beef",
            syntax::hex_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::hex;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0x` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find_if(
                std::next(str.begin(), 2), str.end(), [](const char c) {
                    return c != '0' && c != '_';
                }));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0x0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    // prefix zero and _ is removed. check if it uses upper/lower case.
    // if both upper and lower case letters are found, set upper=true.
    const auto lower_not_found = std::find_if(str.begin(), str.end(),
        [](const char c) { return std::islower(static_cast<int>(c)) != 0; }) == str.end();
    const auto upper_found = std::find_if(str.begin(), str.end(),
        [](const char c) { return std::isupper(static_cast<int>(c)) != 0; }) != str.end();
    fmt.uppercase = lower_not_found || upper_found;

    const auto val = TC::parse_int(str, source_location(region(loc)), 16);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_dec_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::dec_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_dec_integer: "
            "invalid integer: dec_int must be like: 42, 123_456_789",
            syntax::dec_int(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt = integer_format::dec;
    fmt.width = str.size() - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    auto src = source_location(region(loc));
    const auto val = TC::parse_int(str, src, 10);
    if(val.is_err())
    {
        loc = first;
        return err(val.as_err());
    }

    // ----------------------------------------------------------------------
    // parse suffix (extension)

    if(spec.ext_num_suffix && loc.current() == '_')
    {
        const auto sfx_reg = syntax::num_suffix(spec).scan(loc);
        if( ! sfx_reg.is_ok())
        {
            loc = first;
            return err(make_error_info("toml::parse_dec_integer: "
                "invalid suffix: should be `_ non-digit-graph (graph | _graph)`",
                source_location(region(loc)), "here"));
        }
        auto sfx = sfx_reg.as_string();
        assert( ! sfx.empty() && sfx.front() == '_');
        sfx.erase(sfx.begin()); // remove the first `_`

        fmt.suffix = sfx;
    }

    return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    if( ! loc.eof() && (loc.current() == '+' || loc.current() == '-'))
    {
        // skip +/- to diagnose +0xDEADBEEF or -0b0011 (invalid).
        // without this, +0xDEAD_BEEF will be parsed as a decimal int and
        // unexpected "xDEAD_BEEF" will appear after integer "+0".
        loc.advance();
    }

    if( ! loc.eof() && loc.current() == '0')
    {
        loc.advance();
        if(loc.eof())
        {
            // `[+-]?0`. parse as an decimal integer.
            loc = first;
            return parse_dec_integer(loc, ctx);
        }

        const auto prefix = loc.current();
        auto prefix_src = source_location(region(loc));

        loc = first;

        if(prefix == 'b') {return parse_bin_integer(loc, ctx);}
        if(prefix == 'o') {return parse_oct_integer(loc, ctx);}
        if(prefix == 'x') {return parse_hex_integer(loc, ctx);}

        if(std::isdigit(prefix))
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_integer: "
                "leading zero in an decimal integer is not allowed",
                std::move(src), "leading zero"));
        }
    }

    loc = first;
    return parse_dec_integer(loc, ctx);
}

/* ============================================================================
 *  ___ _           _   _
 * | __| |___  __ _| |_(_)_ _  __ _
 * | _|| / _ \/ _` |  _| | ' \/ _` |
 * |_| |_\___/\__,_|\__|_|_||_\__, |
 *                            |___/
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_floating(location& loc, const context<TC>& ctx)
{
    using floating_type = typename basic_value<TC>::floating_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    bool is_hex = false;
    std::string str;
    region reg;
    if(spec.ext_hex_float && sequence(character('0'), character('x')).scan(loc).is_ok())
    {
        loc = first;
        is_hex = true;

        reg = syntax::hex_floating(spec).scan(loc);
        if( ! reg.is_ok())
        {
            return err(make_syntax_error("toml::parse_floating: "
                "invalid hex floating: float must be like: 0xABCp-3f",
                syntax::floating(spec), loc));
        }
        str = reg.as_string();
    }
    else
    {
        reg = syntax::floating(spec).scan(loc);
        if( ! reg.is_ok())
        {
            return err(make_syntax_error("toml::parse_floating: "
                "invalid floating: float must be like: -3.14159_26535, 6.022e+23, "
                "inf, or nan (lowercase).", syntax::floating(spec), loc));
        }
        str = reg.as_string();
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    floating_format_info fmt;

    if(is_hex)
    {
        fmt.fmt = floating_format::hex;
    }
    else
    {
        // since we already checked that the string conforms the TOML standard.
        if(std::find(str.begin(), str.end(), 'e') != str.end() ||
           std::find(str.begin(), str.end(), 'E') != str.end())
        {
            fmt.fmt = floating_format::scientific; // use exponent part
        }
        else
        {
            fmt.fmt = floating_format::fixed; // do not use exponent part
        }
    }

    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    floating_type val{0};

    if(str == "inf" || str == "+inf")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_infinity)
        {
            val = std::numeric_limits<floating_type>::infinity();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: inf value found"
                " but the current environment does not support inf. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: inf is not supported"));
        }
    }
    else if(str == "-inf")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_infinity)
        {
            val = -std::numeric_limits<floating_type>::infinity();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: inf value found"
                " but the current environment does not support inf. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: inf is not supported"));
        }
    }
    else if(str == "nan" || str == "+nan")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_quiet_NaN)
        {
            val = std::numeric_limits<floating_type>::quiet_NaN();
        }
        else TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_signaling_NaN)
        {
            val = std::numeric_limits<floating_type>::signaling_NaN();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: NaN value found"
                " but the current environment does not support NaN. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: NaN is not supported"));
        }
    }
    else if(str == "-nan")
    {
        using std::copysign;
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_quiet_NaN)
        {
            val = copysign(std::numeric_limits<floating_type>::quiet_NaN(), floating_type(-1));
        }
        else TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_signaling_NaN)
        {
            val = copysign(std::numeric_limits<floating_type>::signaling_NaN(), floating_type(-1));
        }
        else
        {
            return err(make_error_info("toml::parse_floating: NaN value found"
                " but the current environment does not support NaN. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: NaN is not supported"));
        }
    }
    else
    {
        // set precision
        const auto has_sign = ! str.empty() && (str.front() == '+' || str.front() == '-');
        const auto decpoint = std::find(str.begin(), str.end(), '.');
        const auto exponent = std::find_if(str.begin(), str.end(),
            [](const char c) { return c == 'e' || c == 'E'; });
        if(decpoint != str.end() && exponent != str.end())
        {
            assert(decpoint < exponent);
        }

        if(fmt.fmt == floating_format::scientific)
        {
            // total width
            fmt.prec = static_cast<std::size_t>(std::distance(str.begin(), exponent));
            if(has_sign)
            {
                fmt.prec -= 1;
            }
            if(decpoint != str.end())
            {
                fmt.prec -= 1;
            }
        }
        else if(fmt.fmt == floating_format::hex)
        {
            fmt.prec = std::numeric_limits<floating_type>::max_digits10;
        }
        else
        {
            // width after decimal point
            fmt.prec = static_cast<std::size_t>(std::distance(std::next(decpoint), exponent));
        }

        auto src = source_location(region(loc));
        const auto res = TC::parse_float(str, src, is_hex);
        if(res.is_ok())
        {
            val = res.as_ok();
        }
        else
        {
            return err(res.as_err());
        }
    }

    // ----------------------------------------------------------------------
    // parse suffix (extension)

    if(spec.ext_num_suffix && loc.current() == '_')
    {
        const auto sfx_reg = syntax::num_suffix(spec).scan(loc);
        if( ! sfx_reg.is_ok())
        {
            auto src = source_location(region(loc));
            loc = first;
            return err(make_error_info("toml::parse_floating: "
                "invalid suffix: should be `_ non-digit-graph (graph | _graph)`",
                std::move(src), "here"));
        }
        auto sfx = sfx_reg.as_string();
        assert( ! sfx.empty() && sfx.front() == '_');
        sfx.erase(sfx.begin()); // remove the first `_`

        fmt.suffix = sfx;
    }

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *  ___       _       _   _
 * |   \ __ _| |_ ___| |_(_)_ __  ___
 * | |) / _` |  _/ -_)  _| | '  \/ -_)
 * |___/\__,_|\__\___|\__|_|_|_|_\___|
 */

// all the offset_datetime, local_datetime, local_date parses date part.
template<typename TC>
result<std::tuple<local_date, local_date_format_info, region>, error_info>
parse_local_date_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    local_date_format_info fmt;

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::local_date(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_local_date: "
            "invalid date: date must be like: 1234-05-06, yyyy-mm-dd.",
            syntax::local_date(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();

    // 0123456789
    // yyyy-mm-dd
    const auto year_r  = from_string<int>(str.substr(0, 4));
    const auto month_r = from_string<int>(str.substr(5, 2));
    const auto day_r   = from_string<int>(str.substr(8, 2));

    if(year_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read year `" + str.substr(0, 4) + "`",
            std::move(src), "here"));
    }
    if(month_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read month `" + str.substr(5, 2) + "`",
            std::move(src), "here"));
    }
    if(day_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read day `" + str.substr(8, 2) + "`",
            std::move(src), "here"));
    }

    const auto year  = year_r.unwrap();
    const auto month = month_r.unwrap();
    const auto day   = day_r.unwrap();

    {
        // We briefly check whether the input date is valid or not.
        //     Actually, because of the historical reasons, there are several
        // edge cases, such as 1582/10/5-1582/10/14 (only in several countries).
        // But here, we do not care about it.
        // It makes the code complicated and there is only low probability
        // that such a specific date is needed in practice. If someone need to
        // validate date accurately, that means that the one need a specialized
        // library for their purpose in another layer.

        const bool is_leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        const auto max_day = [month, is_leap]() {
            if(month == 2)
            {
                return is_leap ? 29 : 28;
            }
            if(month == 4 || month == 6 || month == 9 || month == 11)
            {
                return 30;
            }
            return 31;
        }();

        if((month < 1 || 12 < month) || (day < 1 || max_day < day))
        {
            auto src = source_location(region(first));
            return err(make_error_info("toml::parse_local_date: invalid date.",
                std::move(src), "month must be 01-12, day must be any of "
                "01-28,29,30,31 depending on the month/year."));
        }
    }

    return ok(std::make_tuple(
            local_date(year, static_cast<month_t>(month - 1), day),
            std::move(fmt), std::move(reg)
        ));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_date(location& loc, const context<TC>& ctx)
{
    auto val_fmt_reg = parse_local_date_only(loc, ctx);
    if(val_fmt_reg.is_err())
    {
        return err(val_fmt_reg.unwrap_err());
    }

    auto val = std::move(std::get<0>(val_fmt_reg.unwrap()));
    auto fmt = std::move(std::get<1>(val_fmt_reg.unwrap()));
    auto reg = std::move(std::get<2>(val_fmt_reg.unwrap()));

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

// all the offset_datetime, local_datetime, local_time parses date part.
template<typename TC>
result<std::tuple<local_time, local_time_format_info, region>, error_info>
parse_local_time_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    local_time_format_info fmt;

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::local_time(spec).scan(loc);
    if( ! reg.is_ok())
    {
        if(spec.v1_1_0_make_seconds_optional)
        {
            return err(make_syntax_error("toml::parse_local_time: "
                "invalid time: time must be HH:MM(:SS.sss) (seconds are optional)",
                syntax::local_time(spec), loc));
        }
        else
        {
            return err(make_syntax_error("toml::parse_local_time: "
                "invalid time: time must be HH:MM:SS(.sss) (subseconds are optional)",
                syntax::local_time(spec), loc));
        }
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();

    // at least we have HH:MM.
    // 01234
    // HH:MM
    const auto hour_r   = from_string<int>(str.substr(0, 2));
    const auto minute_r = from_string<int>(str.substr(3, 2));

    if(hour_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read hour `" + str.substr(0, 2) + "`",
            std::move(src), "here"));
    }
    if(minute_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read minute `" + str.substr(3, 2) + "`",
            std::move(src), "here"));
    }

    const auto hour   = hour_r.unwrap();
    const auto minute = minute_r.unwrap();

    if((hour < 0 || 24 <= hour) || (minute < 0 || 60 <= minute))
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: invalid time.",
            std::move(src), "hour must be 00-23, minute must be 00-59."));
    }

    // -----------------------------------------------------------------------
    // we have hour and minute.
    // Since toml v1.1.0, second and subsecond part becomes optional.
    // Check the version and return if second does not exist.

    if(str.size() == 5 && spec.v1_1_0_make_seconds_optional)
    {
        fmt.has_seconds = false;
        fmt.subsecond_precision = 0;
        return ok(std::make_tuple(local_time(hour, minute, 0), std::move(fmt), std::move(reg)));
    }
    assert(str.at(5) == ':');

    // we have at least `:SS` part. `.subseconds` are optional.

    // 0         1
    // 012345678901234
    // HH:MM:SS.subsec
    const auto sec_r = from_string<int>(str.substr(6, 2));
    if(sec_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read second `" + str.substr(6, 2) + "`",
            std::move(src), "here"));
    }
    const auto sec = sec_r.unwrap();

    if(sec < 0 || 60 < sec) // :60 is allowed
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: invalid time.",
                    std::move(src), "second must be 00-60."));
    }

    if(str.size() == 8)
    {
        fmt.has_seconds = true;
        fmt.subsecond_precision = 0;
        return ok(std::make_tuple(local_time(hour, minute, sec), std::move(fmt), std::move(reg)));
    }

    assert(str.at(8) == '.');

    auto secfrac = str.substr(9, str.size() - 9);

    fmt.has_seconds = true;
    fmt.subsecond_precision = secfrac.size();

    while(secfrac.size() < 9)
    {
        secfrac += '0';
    }
    assert(9 <= secfrac.size());
    const auto ms_r = from_string<int>(secfrac.substr(0, 3));
    const auto us_r = from_string<int>(secfrac.substr(3, 3));
    const auto ns_r = from_string<int>(secfrac.substr(6, 3));

    if(ms_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read milliseconds `" + secfrac.substr(0, 3) + "`",
            std::move(src), "here"));
    }
    if(us_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read microseconds`" + str.substr(3, 3) + "`",
            std::move(src), "here"));
    }
    if(ns_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read nanoseconds`" + str.substr(6, 3) + "`",
            std::move(src), "here"));
    }
    const auto ms = ms_r.unwrap();
    const auto us = us_r.unwrap();
    const auto ns = ns_r.unwrap();

    return ok(std::make_tuple(local_time(hour, minute, sec, ms, us, ns), std::move(fmt), std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_time(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    auto val_fmt_reg = parse_local_time_only(loc, ctx);
    if(val_fmt_reg.is_err())
    {
        return err(val_fmt_reg.unwrap_err());
    }

    auto val = std::move(std::get<0>(val_fmt_reg.unwrap()));
    auto fmt = std::move(std::get<1>(val_fmt_reg.unwrap()));
    auto reg = std::move(std::get<2>(val_fmt_reg.unwrap()));

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_datetime(location& loc, const context<TC>& ctx)
{
    using char_type = location::char_type;

    const auto first = loc;

    local_datetime_format_info fmt;

    // ----------------------------------------------------------------------

    auto date_fmt_reg = parse_local_date_only(loc, ctx);
    if(date_fmt_reg.is_err())
    {
        return err(date_fmt_reg.unwrap_err());
    }

    if(loc.current() == char_type('T'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::upper_T;
    }
    else if(loc.current() == char_type('t'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::lower_t;
    }
    else if(loc.current() == char_type(' '))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::space;
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_local_datetime: "
            "expect date-time delimiter `T`, `t` or ` `(space).",
            std::move(src), "here"));
    }

    auto time_fmt_reg = parse_local_time_only(loc, ctx);
    if(time_fmt_reg.is_err())
    {
        return err(time_fmt_reg.unwrap_err());
    }

    fmt.has_seconds         = std::get<1>(time_fmt_reg.unwrap()).has_seconds;
    fmt.subsecond_precision = std::get<1>(time_fmt_reg.unwrap()).subsecond_precision;

    // ----------------------------------------------------------------------

    region reg(first, loc);
    local_datetime val(std::get<0>(date_fmt_reg.unwrap()),
                       std::get<0>(time_fmt_reg.unwrap()));

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_offset_datetime(location& loc, const context<TC>& ctx)
{
    using char_type = location::char_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    offset_datetime_format_info fmt;

    // ----------------------------------------------------------------------
    // date part

    auto date_fmt_reg = parse_local_date_only(loc, ctx);
    if(date_fmt_reg.is_err())
    {
        return err(date_fmt_reg.unwrap_err());
    }

    // ----------------------------------------------------------------------
    // delimiter

    if(loc.current() == char_type('T'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::upper_T;
    }
    else if(loc.current() == char_type('t'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::lower_t;
    }
    else if(loc.current() == char_type(' '))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::space;
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_offset_datetime: "
            "expect date-time delimiter `T` or ` `(space).", std::move(src), "here"
        ));
    }

    // ----------------------------------------------------------------------
    // time part

    auto time_fmt_reg = parse_local_time_only(loc, ctx);
    if(time_fmt_reg.is_err())
    {
        return err(time_fmt_reg.unwrap_err());
    }

    fmt.has_seconds         = std::get<1>(time_fmt_reg.unwrap()).has_seconds;
    fmt.subsecond_precision = std::get<1>(time_fmt_reg.unwrap()).subsecond_precision;

    // ----------------------------------------------------------------------
    // offset part

    const auto ofs_reg = syntax::time_offset(spec).scan(loc);
    if( ! ofs_reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_offset_datetime: "
            "invalid offset: offset must be like: Z, +01:00, or -10:00.",
            syntax::time_offset(spec), loc));
    }

    const auto ofs_str = ofs_reg.as_string();

    time_offset offset(0, 0);

    assert(ofs_str.size() != 0);

    if(ofs_str.at(0) == char_type('+') || ofs_str.at(0) == char_type('-'))
    {
        const auto hour_r   = from_string<int>(ofs_str.substr(1, 2));
        const auto minute_r = from_string<int>(ofs_str.substr(4, 2));
        if(hour_r.is_err())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_offset_datetime: "
                "Failed to read offset hour part", std::move(src), "here"));
        }
        if(minute_r.is_err())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_offset_datetime: "
                "Failed to read offset minute part", std::move(src), "here"));
        }
        const auto hour   = hour_r.unwrap();
        const auto minute = minute_r.unwrap();

        if(ofs_str.at(0) == '+')
        {
            offset = time_offset(hour, minute);
        }
        else
        {
            offset = time_offset(-hour, -minute);
        }
    }
    else
    {
        assert(ofs_str.at(0) == char_type('Z') || ofs_str.at(0) == char_type('z'));
    }

    if (offset.hour   < -24 || 24 < offset.hour ||
        offset.minute < -60 || 60 < offset.minute)
    {
        return err(make_error_info("toml::parse_offset_datetime: "
            "too large offset: |hour| <= 24, |minute| <= 60",
            source_location(region(first, loc)), "here"));
    }


    // ----------------------------------------------------------------------

    region reg(first, loc);
    offset_datetime val(local_datetime(std::get<0>(date_fmt_reg.unwrap()),
                                       std::get<0>(time_fmt_reg.unwrap())),
                                       offset);

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *   ___ _       _
 *  / __| |_ _ _(_)_ _  __ _
 *  \__ \  _| '_| | ' \/ _` |
 *  |___/\__|_| |_|_||_\__, |
 *                     |___/
 */

template<typename TC>
result<typename basic_value<TC>::string_type, error_info>
parse_utf8_codepoint(const region& reg)
{
    using string_type = typename basic_value<TC>::string_type;
    using char_type = typename string_type::value_type;

    // assert(reg.as_lines().size() == 1); // XXX heavy check

    const auto str = reg.as_string();
    assert( ! str.empty());
    assert(str.front() == 'u' || str.front() == 'U' || str.front() == 'x');

    std::uint_least32_t codepoint;
    std::istringstream iss(str.substr(1));
    iss >> std::hex >> codepoint;

    const auto to_char = [](const std::uint_least32_t i) noexcept -> char_type {
        const auto uc = static_cast<unsigned char>(i & 0xFF);
        return cxx::bit_cast<char_type>(uc);
    };

    string_type character;
    if(codepoint < 0x80) // U+0000 ... U+0079 ; just an ASCII.
    {
        character += static_cast<char>(codepoint);
    }
    else if(codepoint < 0x800) //U+0080 ... U+07FF
    {
        // 110yyyyx 10xxxxxx; 0x3f == 0b0011'1111
        character += to_char(0xC0|(codepoint >> 6  ));
        character += to_char(0x80|(codepoint & 0x3F));
    }
    else if(codepoint < 0x10000) // U+0800...U+FFFF
    {
        if(0xD800 <= codepoint && codepoint <= 0xDFFF)
        {
            auto src = source_location(reg);
            return err(make_error_info("toml::parse_utf8_codepoint: "
                "[0xD800, 0xDFFF] is not a valid UTF-8",
                std::move(src), "here"));
        }
        assert(codepoint < 0xD800 || 0xDFFF < codepoint);
        // 1110yyyy 10yxxxxx 10xxxxxx
        character += to_char(0xE0| (codepoint >> 12));
        character += to_char(0x80|((codepoint >> 6 ) & 0x3F));
        character += to_char(0x80|((codepoint      ) & 0x3F));
    }
    else if(codepoint < 0x110000) // U+010000 ... U+10FFFF
    {
        // 11110yyy 10yyxxxx 10xxxxxx 10xxxxxx
        character += to_char(0xF0| (codepoint >> 18));
        character += to_char(0x80|((codepoint >> 12) & 0x3F));
        character += to_char(0x80|((codepoint >> 6 ) & 0x3F));
        character += to_char(0x80|((codepoint      ) & 0x3F));
    }
    else // out of UTF-8 region
    {
        auto src = source_location(reg);
        return err(make_error_info("toml::parse_utf8_codepoint: "
            "input codepoint is too large.",
            std::move(src), "must be in range [0x00, 0x10FFFF]"));
    }
    return ok(character);
}

template<typename TC>
result<typename basic_value<TC>::string_type, error_info>
parse_escape_sequence(location& loc, const context<TC>& ctx)
{
    using string_type = typename basic_value<TC>::string_type;
    using char_type = typename string_type::value_type;

    const auto& spec = ctx.toml_spec();

    assert( ! loc.eof());
    assert(loc.current() == '\\');
    loc.advance(); // consume the first backslash

    string_type retval;

    if     (loc.current() == '\\') { retval += char_type('\\'); loc.advance(); }
    else if(loc.current() == '"')  { retval += char_type('\"'); loc.advance(); }
    else if(loc.current() == 'b')  { retval += char_type('\b'); loc.advance(); }
    else if(loc.current() == 'f')  { retval += char_type('\f'); loc.advance(); }
    else if(loc.current() == 'n')  { retval += char_type('\n'); loc.advance(); }
    else if(loc.current() == 'r')  { retval += char_type('\r'); loc.advance(); }
    else if(loc.current() == 't')  { retval += char_type('\t'); loc.advance(); }
    else if(spec.v1_1_0_add_escape_sequence_e && loc.current() == 'e')
    {
        retval += char_type('\x1b');
        loc.advance();
    }
    else if(spec.v1_1_0_add_escape_sequence_x && loc.current() == 'x')
    {
        auto scanner = sequence(character('x'), repeat_exact(2, syntax::hexdig(spec)));
        const auto reg = scanner.scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\xhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else if(loc.current() == 'u')
    {
        auto scanner = sequence(character('u'), repeat_exact(4, syntax::hexdig(spec)));
        const auto reg = scanner.scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\uhhhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else if(loc.current() == 'U')
    {
        auto scanner = sequence(character('U'), repeat_exact(8, syntax::hexdig(spec)));
        const auto reg = scanner.scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\Uhhhhhhhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else
    {
        auto src = source_location(region(loc));
        std::string escape_seqs = "allowed escape seqs: \\\\, \\\", \\b, \\f, \\n, \\r, \\t";
        if(spec.v1_1_0_add_escape_sequence_e)
        {
            escape_seqs += ", \\e";
        }
        if(spec.v1_1_0_add_escape_sequence_x)
        {
            escape_seqs += ", \\xhh";
        }
        escape_seqs += ", \\uhhhh, or \\Uhhhhhhhh";

        return err(make_error_info("toml::parse_escape_sequence: "
               "unknown escape sequence.", std::move(src), escape_seqs));
    }
    return ok(retval);
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_ml_basic_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    string_format_info fmt;
    fmt.fmt = string_format::multiline_basic;

    auto reg = syntax::ml_basic_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_ml_basic_string: "
            "invalid string format",
            syntax::ml_basic_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    // we already checked that it starts with """ and ends with """.
    assert(str.substr(0, 3) == "\"\"\"");
    str.erase(0, 3);

    assert(str.size() >= 3);
    assert(str.substr(str.size()-3, 3) == "\"\"\"");
    str.erase(str.size()-3, 3);

    // the first newline just after """ is trimmed
    if(str.size() >= 1 && str.at(0) == '\n')
    {
        str.erase(0, 1);
        fmt.start_with_newline = true;
    }
    else if(str.size() >= 2 && str.at(0) == '\r' && str.at(1) == '\n')
    {
        str.erase(0, 2);
        fmt.start_with_newline = true;
    }

    using string_type = typename basic_value<TC>::string_type;
    string_type val;
    {
        auto iter = str.cbegin();
        while(iter != str.cend())
        {
            if(*iter == '\\') // remove whitespaces around escaped-newline
            {
                // we assume that the string is not too long to copy
                auto loc2 = make_temporary_location(make_string(iter, str.cend()));
                if(syntax::escaped_newline(spec).scan(loc2).is_ok())
                {
                    std::advance(iter, loc2.get_location()); // skip escaped newline and indent
                    // now iter points non-WS char
                    assert(iter == str.end() || (*iter != ' ' && *iter != '\t'));
                }
                else // normal escape seq.
                {
                    auto esc = parse_escape_sequence(loc2, ctx);

                    // syntax does not check its value. the unicode codepoint may be
                    // invalid, e.g. out-of-bound, [0xD800, 0xDFFF]
                    if(esc.is_err())
                    {
                        return err(esc.unwrap_err());
                    }

                    val += esc.unwrap();
                    std::advance(iter, loc2.get_location());
                }
            }
            else // we already checked the syntax. we don't need to check it again.
            {
                val += static_cast<typename string_type::value_type>(*iter);
                ++iter;
            }
        }
    }

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<std::pair<typename basic_value<TC>::string_type, region>, error_info>
parse_basic_string_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::basic_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_basic_string: "
            "invalid string format",
            syntax::basic_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.back() == '\"');
    str.pop_back();
    assert(str.at(0) == '\"');
    str.erase(0, 1);

    using string_type = typename basic_value<TC>::string_type;
    using char_type   = typename string_type::value_type;
    string_type val;

    {
        auto iter = str.begin();
        while(iter != str.end())
        {
            if(*iter == '\\')
            {
                auto loc2 = make_temporary_location(make_string(iter, str.end()));

                auto esc = parse_escape_sequence(loc2, ctx);

                // syntax does not check its value. the unicode codepoint may be
                // invalid, e.g. out-of-bound, [0xD800, 0xDFFF]
                if(esc.is_err())
                {
                    return err(esc.unwrap_err());
                }

                val += esc.unwrap();
                std::advance(iter, loc2.get_location());
            }
            else
            {
                val += char_type(*iter); // we already checked the syntax.
                ++iter;
            }
        }
    }
    return ok(std::make_pair(val, reg));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_basic_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    string_format_info fmt;
    fmt.fmt = string_format::basic;

    auto val_res = parse_basic_string_only(loc, ctx);
    if(val_res.is_err())
    {
        return err(std::move(val_res.unwrap_err()));
    }
    auto val = std::move(val_res.unwrap().first );
    auto reg = std::move(val_res.unwrap().second);

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_ml_literal_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    string_format_info fmt;
    fmt.fmt = string_format::multiline_literal;

    auto reg = syntax::ml_literal_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_ml_literal_string: "
            "invalid string format",
            syntax::ml_literal_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.substr(0, 3) == "'''");
    assert(str.substr(str.size()-3, 3) == "'''");
    str.erase(0, 3);
    str.erase(str.size()-3, 3);

    // the first newline just after """ is trimmed
    if(str.size() >= 1 && str.at(0) == '\n')
    {
        str.erase(0, 1);
        fmt.start_with_newline = true;
    }
    else if(str.size() >= 2 && str.at(0) == '\r' && str.at(1) == '\n')
    {
        str.erase(0, 2);
        fmt.start_with_newline = true;
    }

    using string_type = typename basic_value<TC>::string_type;
    string_type val(str.begin(), str.end());

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<std::pair<typename basic_value<TC>::string_type, region>, error_info>
parse_literal_string_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::literal_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_literal_string: "
            "invalid string format",
            syntax::literal_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.back() == '\'');
    str.pop_back();
    assert(str.at(0) == '\'');
    str.erase(0, 1);

    using string_type = typename basic_value<TC>::string_type;
    string_type val(str.begin(), str.end());

    return ok(std::make_pair(std::move(val), std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_literal_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    string_format_info fmt;
    fmt.fmt = string_format::literal;

    auto val_res = parse_literal_string_only(loc, ctx);
    if(val_res.is_err())
    {
        return err(std::move(val_res.unwrap_err()));
    }
    auto val = std::move(val_res.unwrap().first );
    auto reg = std::move(val_res.unwrap().second);

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    if( ! loc.eof() && loc.current() == '"')
    {
        if(literal("\"\"\"").scan(loc).is_ok())
        {
            loc = first;
            return parse_ml_basic_string(loc, ctx);
        }
        else
        {
            loc = first;
            return parse_basic_string(loc, ctx);
        }
    }
    else if( ! loc.eof() && loc.current() == '\'')
    {
        if(literal("'''").scan(loc).is_ok())
        {
            loc = first;
            return parse_ml_literal_string(loc, ctx);
        }
        else
        {
            loc = first;
            return parse_literal_string(loc, ctx);
        }
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_string: "
            "not a string", std::move(src), "here"));
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_null(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    if( ! spec.ext_null_value)
    {
        return err(make_error_info("toml::parse_null: "
            "invalid spec: spec.ext_null_value must be true.",
            source_location(region(loc)), "here"));
    }

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::null_value(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_null: "
            "invalid null: null must be lowercase. ",
            syntax::null_value(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    // ----------------------------------------------------------------------
    // no format info for boolean

    return ok(basic_value<TC>(detail::none_t{}, std::move(reg)));
}

/* ============================================================================
 *   _  __
 *  | |/ /___ _  _
 *  | ' </ -_) || |
 *  |_|\_\___|\_, |
 *            |__/
 */

// non-dotted key.
template<typename TC>
result<typename basic_value<TC>::key_type, error_info>
parse_simple_key(location& loc, const context<TC>& ctx)
{
    using key_type = typename basic_value<TC>::key_type;
    const auto& spec = ctx.toml_spec();

    if(loc.current() == '\"')
    {
        auto str_res = parse_basic_string_only(loc, ctx);
        if(str_res.is_ok())
        {
            return ok(std::move(str_res.unwrap().first));
        }
        else
        {
            return err(std::move(str_res.unwrap_err()));
        }
    }
    else if(loc.current() == '\'')
    {
        auto str_res = parse_literal_string_only(loc, ctx);
        if(str_res.is_ok())
        {
            return ok(std::move(str_res.unwrap().first));
        }
        else
        {
            return err(std::move(str_res.unwrap_err()));
        }
    }

    // bare key.

    if(const auto bare = syntax::unquoted_key(spec).scan(loc))
    {
        return ok(string_conv<key_type>(bare.as_string()));
    }
    else
    {
        std::string postfix;
        if(spec.v1_1_0_allow_non_english_in_bare_keys)
        {
            postfix = "Hint: Not all Unicode characters are allowed as bare key.\n";
        }
        else
        {
            postfix = "Hint: non-ASCII scripts are allowed in toml v1.1.0, but not in v1.0.0.\n";
        }
        return err(make_syntax_error("toml::parse_simple_key: "
            "invalid key: key must be \"quoted\", 'quoted-literal', or bare key.",
            syntax::unquoted_key(spec), loc, postfix));
    }
}

// dotted key become vector of keys
template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_key(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    using key_type = typename basic_value<TC>::key_type;
    std::vector<key_type> keys;
    while( ! loc.eof())
    {
        auto key = parse_simple_key(loc, ctx);
        if( ! key.is_ok())
        {
            return err(key.unwrap_err());
        }
        keys.push_back(std::move(key.unwrap()));

        auto reg = syntax::dot_sep(spec).scan(loc);
        if( ! reg.is_ok())
        {
            break;
        }
    }
    if(keys.empty())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_key: expected a new key, "
                    "but got nothing", std::move(src), "reached EOF"));
    }

    return ok(std::make_pair(std::move(keys), region(first, loc)));
}

// ============================================================================

// forward-decl to implement parse_array and parse_table
template<typename TC>
result<basic_value<TC>, error_info>
parse_value(location&, context<TC>& ctx);

template<typename TC>
result<std::pair<
        std::pair<std::vector<typename basic_value<TC>::key_type>, region>,
        basic_value<TC>
    >, error_info>
parse_key_value_pair(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto key_res = parse_key(loc, ctx);
    if(key_res.is_err())
    {
        loc = first;
        return err(key_res.unwrap_err());
    }

    if( ! syntax::keyval_sep(spec).scan(loc).is_ok())
    {
        auto e = make_syntax_error("toml::parse_key_value_pair: "
            "invalid key value separator `=`", syntax::keyval_sep(spec), loc);
        loc = first;
        return err(std::move(e));
    }

    auto v_res = parse_value(loc, ctx);
    if(v_res.is_err())
    {
        // loc = first;
        return err(v_res.unwrap_err());
    }
    return ok(std::make_pair(std::move(key_res.unwrap()), std::move(v_res.unwrap())));
}

/* ============================================================================
 *    __ _ _ _ _ _ __ _ _  _
 *   / _` | '_| '_/ _` | || |
 *   \__,_|_| |_| \__,_|\_, |
 *                      |__/
 */

// array(and multiline inline table with `{` and `}`) has the following format.
// `[`
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? `,`
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? `,`
// ...
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? (`,`)?
// (ws|newline|comment-line)? `]`
// it skips (ws|newline|comment-line) and returns the token.
template<typename TC>
struct multiline_spacer
{
    using comment_type = typename TC::comment_type;
    bool         newline_found;
    indent_char  indent_type;
    std::int32_t indent;
    comment_type comments;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const multiline_spacer<T>& sp)
{
    os << "{newline="    << sp.newline_found << ", ";
    os << "indent_type=" << sp.indent_type << ", ";
    os << "indent="      << sp.indent << ", ";
    os << "comments="    << sp.comments.size() << "}";
    return os;
}

template<typename TC>
cxx::optional<multiline_spacer<TC>>
skip_multiline_spacer(location& loc, context<TC>& ctx, const bool newline_found = false)
{
    const auto& spec = ctx.toml_spec();

    multiline_spacer<TC> spacer;
    spacer.newline_found = newline_found;
    spacer.indent_type   = indent_char::none;
    spacer.indent        = 0;
    spacer.comments.clear();

    bool spacer_found = false;
    while( ! loc.eof())
    {
        if(auto comm = sequence(syntax::comment(spec), syntax::newline(spec)).scan(loc))
        {
            spacer.newline_found = true;
            auto comment = comm.as_string();
            if( ! comment.empty() && comment.back() == '\n')
            {
                comment.pop_back();
                if (!comment.empty() && comment.back() == '\r')
                {
                    comment.pop_back();
                }
            }

            spacer.comments.push_back(std::move(comment));
            spacer.indent_type = indent_char::none;
            spacer.indent = 0;
            spacer_found = true;
        }
        else if(auto nl = syntax::newline(spec).scan(loc))
        {
            spacer.newline_found = true;
            spacer.comments.clear();
            spacer.indent_type = indent_char::none;
            spacer.indent = 0;
            spacer_found = true;
        }
        else if(auto sp = repeat_at_least(1, character(cxx::bit_cast<location::char_type>(' '))).scan(loc))
        {
            spacer.indent_type = indent_char::space;
            spacer.indent      = static_cast<std::int32_t>(sp.length());
            spacer_found = true;
        }
        else if(auto tabs = repeat_at_least(1, character(cxx::bit_cast<location::char_type>('\t'))).scan(loc))
        {
            spacer.indent_type = indent_char::tab;
            spacer.indent      = static_cast<std::int32_t>(tabs.length());
            spacer_found = true;
        }
        else
        {
            break; // done
        }
    }
    if( ! spacer_found)
    {
        return cxx::make_nullopt();
    }
    return spacer;
}

// not an [[array.of.tables]]. It parses ["this", "type"]
template<typename TC>
result<basic_value<TC>, error_info>
parse_array(location& loc, context<TC>& ctx)
{
    const auto num_errors = ctx.errors().size();

    const auto first = loc;

    if(loc.eof() || loc.current() != '[')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_array: "
                "The next token is not an array", std::move(src), "here"));
    }
    loc.advance();

    typename basic_value<TC>::array_type val;

    array_format_info fmt;
    fmt.fmt = array_format::oneline;
    fmt.indent_type = indent_char::none;

    auto spacer = skip_multiline_spacer(loc, ctx);
    if(spacer.has_value() && spacer.value().newline_found)
    {
        fmt.fmt = array_format::multiline;
    }

    bool comma_found = true;
    while( ! loc.eof())
    {
        if(loc.current() == location::char_type(']'))
        {
            if(spacer.has_value() && spacer.value().newline_found &&
                spacer.value().indent_type != indent_char::none)
            {
                fmt.indent_type    = spacer.value().indent_type;
                fmt.closing_indent = spacer.value().indent;
            }
            break;
        }

        if( ! comma_found)
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_array: "
                    "expected value-separator `,` or closing `]`",
                    std::move(src), "here"));
        }

        if(spacer.has_value() && spacer.value().newline_found &&
            spacer.value().indent_type != indent_char::none)
        {
            fmt.indent_type = spacer.value().indent_type;
            fmt.body_indent = spacer.value().indent;
        }

        if(auto elem_res = parse_value(loc, ctx))
        {
            auto elem = std::move(elem_res.unwrap());

            if(spacer.has_value()) // copy previous comments to value
            {
                elem.comments() = std::move(spacer.value().comments);
            }

            // parse spaces between a value and a comma
            // array = [
            //     42    , # the answer
            //       ^^^^
            //     3.14 # pi
            //   , 2.71 ^^^^
            // ^^
            spacer = skip_multiline_spacer(loc, ctx);
            if(spacer.has_value())
            {
                for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                {
                    elem.comments().push_back(std::move(spacer.value().comments.at(i)));
                }
                if(spacer.value().newline_found)
                {
                    fmt.fmt = array_format::multiline;
                }
            }

            comma_found = character(',').scan(loc).is_ok();

            // parse comment after a comma
            // array = [
            //     42    , # the answer
            //             ^^^^^^^^^^^^
            //     3.14 # pi
            //          ^^^^
            // ]
            auto com_res = parse_comment_line(loc, ctx);
            if(com_res.is_err())
            {
                ctx.report_error(com_res.unwrap_err());
            }

            const bool comment_found = com_res.is_ok() && com_res.unwrap().has_value();
            if(comment_found)
            {
                fmt.fmt = array_format::multiline;
                elem.comments().push_back(com_res.unwrap().value());
            }
            if(comma_found)
            {
                spacer = skip_multiline_spacer(loc, ctx, comment_found);
                if(spacer.has_value() && spacer.value().newline_found)
                {
                    fmt.fmt = array_format::multiline;
                }
            }
            val.push_back(std::move(elem));
        }
        else
        {
            // if err, push error to ctx and try recovery.
            ctx.report_error(std::move(elem_res.unwrap_err()));

            // if it looks like some value, then skip the value.
            // otherwise, it may be a new key-value pair or a new table and
            // the error is "missing closing ]". stop parsing.

            const auto before_skip = loc.get_location();
            skip_value(loc, ctx);
            if(before_skip == loc.get_location()) // cannot skip! break...
            {
                break;
            }
        }
    }

    if(loc.current() != ']')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_array: missing closing bracket `]`",
            std::move(src), "expected `]`, reached EOF"));
    }
    else
    {
        loc.advance();
    }
    // any error reported from this function
    if(num_errors != ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.errors().back());
    }

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, region(first, loc)
        ));
}

/* ============================================================================
 *   _      _ _            _        _    _
 *  (_)_ _ | (_)_ _  ___  | |_ __ _| |__| |___
 *  | | ' \| | | ' \/ -_) |  _/ _` | '_ \ / -_)
 *  |_|_||_|_|_|_||_\___|  \__\__,_|_.__/_\___|
 */

// ----------------------------------------------------------------------------
// insert_value is the most complicated part of the toml spec.
//
// To parse a toml file correctly, we sometimes need to check an exising value
// is appendable or not.
//
// For example while parsing an inline array of tables,
//
// ```toml
// aot = [
//   {a = "foo"},
//   {a = "bar", b = "baz"},
// ]
// ```
//
// this `aot` is appendable until parser reaches to `]`. After that, it becomes
// non-appendable.
//
// On the other hand, a normal array of tables, such as
//
// ```toml
// [[aot]]
// a = "foo"
//
// [[aot]]
// a = "bar"
// b = "baz"
// ```
// This `[[aot]]` is appendable until the parser reaches to the EOF.
//
//
// It becomes a bit more difficult in case of dotted keys.
// In TOML, it is allowed to append a key-value pair to a table that is
// *implicitly* defined by a subtable definitino.
//
// ```toml
// [x.y.z]
// w = 123
//
// [x]
// a = "foo" # OK. x is defined implicitly by `[x.y.z]`.
// ```
//
// But if the table is defined by a dotted keys, it is not appendable.
//
// ```toml
// [x]
// y.z.w = 123
//
// [x.y]
// # ERROR. x.y is already defined by a dotted table in the previous table.
// ```
//
// Also, reopening a table using dotted keys is invalid.
//
// ```toml
// [x.y.z]
// w = 123
//
// [x]
// y.z.v = 42 # ERROR. [x.y.z] is already defined.
// ```
//
//
// ```toml
// [a]
// b.c = "foo"
// b.d = "bar"
// ```
//
//
// ```toml
// a.b = "foo"
// [a]
// c = "bar" # ERROR
// ```
//
// In summary,
// - a table must be defined only once.
// - assignment to an exising table is possible only when:
//   - defining a subtable [x.y] to an existing table [x].
//   - defining supertable [x] explicitly after [x.y].
//   - adding dotted keys in the same table.

enum class inserting_value_kind : std::uint8_t
{
    std_table,   // insert [standard.table]
    array_table, // insert [[array.of.tables]]
    dotted_keys  // insert a.b.c = "this"
};

template<typename TC>
result<basic_value<TC>*, error_info>
insert_value(const inserting_value_kind kind,
    typename basic_value<TC>::table_type* current_table_ptr,
    const std::vector<typename basic_value<TC>::key_type>& keys, region key_reg,
    basic_value<TC> val)
{
    using value_type = basic_value<TC>;
    using array_type = typename basic_value<TC>::array_type;
    using table_type = typename basic_value<TC>::table_type;

    auto key_loc = source_location(key_reg);

    assert( ! keys.empty());

    // dotted key can insert to dotted key tables defined at the same level.
    // dotted key can NOT reopen a table even if it is implcitly-defined one.
    //
    // [x.y.z] # define x and x.y implicitly.
    // a = 42
    //
    // [x] # reopening implcitly defined table
    // r.s.t = 3.14 # VALID r and r.s are new tables.
    // r.s.u = 2.71 # VALID r and r.s are dotted-key tables. valid.
    //
    // y.z.b = "foo" # INVALID x.y.z are multiline table, not a dotted key.
    // y.c   = "bar" # INVALID x.y is implicit multiline table, not a dotted key.

    // a table cannot reopen dotted-key tables.
    //
    // [t1]
    // t2.t3.v = 0
    // [t1.t2] # INVALID t1.t2 is defined as a dotted-key table.

    for(std::size_t i=0; i<keys.size(); ++i)
    {
        const auto& key = keys.at(i);
        table_type& current_table = *current_table_ptr;

        if(i+1 < keys.size()) // there are more keys. go down recursively...
        {
            const auto found = current_table.find(key);
            if(found == current_table.end()) // not found. add new table
            {
                table_format_info fmt;
                fmt.indent_type = indent_char::none;
                if(kind == inserting_value_kind::dotted_keys)
                {
                    fmt.fmt = table_format::dotted;
                }
                else // table / array of tables
                {
                    fmt.fmt = table_format::implicit;
                }
                current_table.emplace(key, value_type(
                    table_type{}, fmt, std::vector<std::string>{}, key_reg));

                assert(current_table.at(key).is_table());
                current_table_ptr = std::addressof(current_table.at(key).as_table());
            }
            else if (found->second.is_table())
            {
                const auto fmt = found->second.as_table_fmt().fmt;
                if(fmt == table_format::oneline || fmt == table_format::multiline_oneline)
                {
                    // foo = {bar = "baz"} or foo = { \n bar = "baz" \n }
                    return err(make_error_info("toml::insert_value: "
                        "failed to insert a value: inline table is immutable",
                        key_loc, "inserting this",
                        found->second.location(), "to this table"));
                }
                // dotted key cannot reopen a table.
                if(kind ==inserting_value_kind::dotted_keys && fmt != table_format::dotted)
                {
                    return err(make_error_info("toml::insert_value: "
                        "reopening a table using dotted keys",
                        key_loc, "dotted key cannot reopen a table",
                        found->second.location(), "this table is already closed"));
                }
                assert(found->second.is_table());
                current_table_ptr = std::addressof(found->second.as_table());
            }
            else if(found->second.is_array_of_tables())
            {
                // aot = [{this = "type", of = "aot"}] # cannot be reopened
                if(found->second.as_array_fmt().fmt != array_format::array_of_tables)
                {
                    return err(make_error_info("toml::insert_value:"
                        "inline array of tables are immutable",
                        key_loc, "inserting this",
                        found->second.location(), "inline array of tables"));
                }
                // appending to [[aot]]

                if(kind == inserting_value_kind::dotted_keys)
                {
                    // [[array.of.tables]]
                    // [array.of]          # reopening supertable is okay
                    // tables.x = "foo"    # appending `x` to the first table
                    return err(make_error_info("toml::insert_value:"
                        "dotted key cannot reopen an array-of-tables",
                        key_loc, "inserting this",
                        found->second.location(), "to this array-of-tables."));
                }

                // insert_value_by_dotkeys::std_table
                // [[array.of.tables]]
                // [array.of.tables.subtable] # appending to the last aot
                //
                // insert_value_by_dotkeys::array_table
                // [[array.of.tables]]
                // [[array.of.tables.subtable]] # appending to the last aot
                auto& current_array_table = found->second.as_array().back();

                assert(current_array_table.is_table());
                current_table_ptr = std::addressof(current_array_table.as_table());
            }
            else
            {
                return err(make_error_info("toml::insert_value: "
                    "failed to insert a value, value already exists",
                    key_loc, "while inserting this",
                    found->second.location(), "non-table value already exists"));
            }
        }
        else // this is the last key. insert a new value.
        {
            switch(kind)
            {
                case inserting_value_kind::dotted_keys:
                {
                    if(current_table.find(key) != current_table.end())
                    {
                        return err(make_error_info("toml::insert_value: "
                            "failed to insert a value, value already exists",
                            key_loc, "inserting this",
                            current_table.at(key).location(), "but value already exists"));
                    }
                    current_table.emplace(key, std::move(val));
                    return ok(std::addressof(current_table.at(key)));
                }
                case inserting_value_kind::std_table:
                {
                    // defining a new table or reopening supertable
                    auto found = current_table.find(key);
                    if(found == current_table.end()) // define a new aot
                    {
                        current_table.emplace(key, std::move(val));
                        return ok(std::addressof(current_table.at(key)));
                    }
                    else // the table is already defined, reopen it
                    {
                        // assigning a [std.table]. it must be an implicit table.
                        auto& target = found->second;
                        if( ! target.is_table() || // could be an array-of-tables
                            target.as_table_fmt().fmt != table_format::implicit)
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert a table, table already defined",
                                key_loc, "inserting this",
                                target.location(), "this table is explicitly defined"));
                        }

                        // merge table
                        for(const auto& kv : val.as_table())
                        {
                            if(target.contains(kv.first))
                            {
                                // [x.y.z]
                                // w = "foo"
                                // [x]
                                // y = "bar"
                                return err(make_error_info("toml::insert_value: "
                                    "failed to insert a table, table keys conflict to each other",
                                    key_loc, "inserting this table",
                                    kv.second.location(), "having this value",
                                    target.at(kv.first).location(), "already defined here"));
                            }
                            else
                            {
                                target[kv.first] = kv.second;
                            }
                        }
                        // change implicit -> explicit
                        target.as_table_fmt().fmt = table_format::multiline;
                        // change definition region
                        change_region_of_value(target, val);

                        return ok(std::addressof(current_table.at(key)));
                    }
                }
                case inserting_value_kind::array_table:
                {
                    auto found = current_table.find(key);
                    if(found == current_table.end()) // define a new aot
                    {
                        array_format_info fmt;
                        fmt.fmt = array_format::array_of_tables;
                        fmt.indent_type = indent_char::none;

                        current_table.emplace(key, value_type(
                                array_type{ std::move(val) }, std::move(fmt),
                                std::vector<std::string>{}, std::move(key_reg)
                            ));

                        assert( ! current_table.at(key).as_array().empty());
                        return ok(std::addressof(current_table.at(key).as_array().back()));
                    }
                    else // the array is already defined, append to it
                    {
                        if( ! found->second.is_array_of_tables())
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert an array of tables, value already exists",
                                key_loc, "while inserting this",
                                found->second.location(), "non-table value already exists"));
                        }
                        if(found->second.as_array_fmt().fmt != array_format::array_of_tables)
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert a table, inline array of tables is immutable",
                                key_loc, "while inserting this",
                                found->second.location(), "this is inline array-of-tables"));
                        }
                        found->second.as_array().push_back(std::move(val));
                        assert( ! current_table.at(key).as_array().empty());
                        return ok(std::addressof(current_table.at(key).as_array().back()));
                    }
                }
                default: {assert(false);}
            }
        }
    }
    return err(make_error_info("toml::insert_key: no keys found",
                std::move(key_loc), "here"));
}

// ----------------------------------------------------------------------------

template<typename TC>
result<basic_value<TC>, error_info>
parse_inline_table(location& loc, context<TC>& ctx)
{
    using table_type = typename basic_value<TC>::table_type;

    const auto num_errors = ctx.errors().size();

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    if(loc.eof() || loc.current() != '{')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_inline_table: "
            "The next token is not an inline table", std::move(src), "here"));
    }
    loc.advance();

    table_type table;
    table_format_info fmt;
    fmt.fmt = table_format::oneline;
    fmt.indent_type = indent_char::none;

    cxx::optional<multiline_spacer<TC>> spacer(cxx::make_nullopt());

    if(spec.v1_1_0_allow_newlines_in_inline_tables)
    {
        spacer = skip_multiline_spacer(loc, ctx);
        if(spacer.has_value() && spacer.value().newline_found)
        {
            fmt.fmt = table_format::multiline_oneline;
        }
    }
    else
    {
        skip_whitespace(loc, ctx);
    }

    bool still_empty = true;
    bool comma_found = false;
    while( ! loc.eof())
    {
        // closing!
        if(loc.current() == '}')
        {
            if(comma_found && ! spec.v1_1_0_allow_trailing_comma_in_inline_tables)
            {
                auto src = source_location(region(loc));
                return err(make_error_info("toml::parse_inline_table: trailing "
                    "comma is not allowed in TOML-v1.0.0)", std::move(src), "here"));
            }

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                if(spacer.has_value() && spacer.value().newline_found &&
                    spacer.value().indent_type != indent_char::none)
                {
                    fmt.indent_type    = spacer.value().indent_type;
                    fmt.closing_indent = spacer.value().indent;
                }
            }
            break;
        }

        // if we already found a value and didn't found `,` nor `}`, error.
        if( ! comma_found && ! still_empty)
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_inline_table: "
                    "expected value-separator `,` or closing `}`",
                    std::move(src), "here"));
        }

        // parse indent.
        if(spacer.has_value() && spacer.value().newline_found &&
            spacer.value().indent_type != indent_char::none)
        {
            fmt.indent_type = spacer.value().indent_type;
            fmt.body_indent = spacer.value().indent;
        }

        still_empty = false; // parsing a value...
        if(auto kv_res = parse_key_value_pair<TC>(loc, ctx))
        {
            auto keys    = std::move(kv_res.unwrap().first.first);
            auto key_reg = std::move(kv_res.unwrap().first.second);
            auto val     = std::move(kv_res.unwrap().second);

            auto ins_res = insert_value(inserting_value_kind::dotted_keys,
                std::addressof(table), keys, std::move(key_reg), std::move(val));
            if(ins_res.is_err())
            {
                ctx.report_error(std::move(ins_res.unwrap_err()));
                // we need to skip until the next value (or end of the table)
                // because we don't have valid kv pair.
                while( ! loc.eof())
                {
                    const auto c = loc.current();
                    if(c == ',' || c == '\n' || c == '}')
                    {
                        comma_found = (c == ',');
                        break;
                    }
                    loc.advance();
                }
                continue;
            }

            // if comment line follows immediately(without newline) after `,`, then
            // the comment is for the elem. we need to check if comment follows `,`.
            //
            // (key) = (val) (ws|newline|comment-line)? `,` (ws)? (comment)?

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                if(spacer.has_value()) // copy previous comments to value
                {
                    for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                    {
                        ins_res.unwrap()->comments().push_back(spacer.value().comments.at(i));
                    }
                }
                spacer = skip_multiline_spacer(loc, ctx);
                if(spacer.has_value())
                {
                    for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                    {
                        ins_res.unwrap()->comments().push_back(spacer.value().comments.at(i));
                    }
                    if(spacer.value().newline_found)
                    {
                        fmt.fmt = table_format::multiline_oneline;
                        if(spacer.value().indent_type != indent_char::none)
                        {
                            fmt.indent_type = spacer.value().indent_type;
                            fmt.body_indent = spacer.value().indent;
                        }
                    }
                }
            }
            else
            {
                skip_whitespace(loc, ctx);
            }

            comma_found = character(',').scan(loc).is_ok();

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                auto com_res = parse_comment_line(loc, ctx);
                if(com_res.is_err())
                {
                    ctx.report_error(com_res.unwrap_err());
                }
                const bool comment_found = com_res.is_ok() && com_res.unwrap().has_value();
                if(comment_found)
                {
                    fmt.fmt = table_format::multiline_oneline;
                    ins_res.unwrap()->comments().push_back(com_res.unwrap().value());
                }
                if(comma_found)
                {
                    spacer = skip_multiline_spacer(loc, ctx, comment_found);
                    if(spacer.has_value() && spacer.value().newline_found)
                    {
                        fmt.fmt = table_format::multiline_oneline;
                    }
                }
            }
            else
            {
                skip_whitespace(loc, ctx);
            }
        }
        else
        {
            ctx.report_error(std::move(kv_res.unwrap_err()));
            while( ! loc.eof())
            {
                if(loc.current() == '}')
                {
                    break;
                }
                if( ! spec.v1_1_0_allow_newlines_in_inline_tables && loc.current() == '\n')
                {
                    break;
                }
                loc.advance();
            }
            break;
        }
    }

    if(loc.current() != '}')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_inline_table: "
            "missing closing bracket `}`",
            std::move(src), "expected `}`, reached line end"));
    }
    else
    {
        loc.advance(); // skip }
    }

    // any error reported from this function
    if(num_errors < ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.pop_last_error());
    }

    basic_value<TC> retval(
        std::move(table), std::move(fmt), {}, region(first, loc));

    return ok(std::move(retval));
}

/* ============================================================================
 *            _
 *  __ ____ _| |_  _ ___
 *  \ V / _` | | || / -_)
 *   \_/\__,_|_|\_,_\___|
 */

template<typename TC>
result<value_t, error_info>
guess_number_type(const location& first, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    location loc = first;

    if(syntax::offset_datetime(spec).scan(loc).is_ok())
    {
        return ok(value_t::offset_datetime);
    }
    loc = first;

    if(syntax::local_datetime(spec).scan(loc).is_ok())
    {
        const auto curr = loc.current();
        // if offset_datetime contains bad offset, it syntax::offset_datetime
        // fails to scan it.
        if(curr == '+' || curr == '-')
        {
            return err(make_syntax_error("bad offset: must be [+-]HH:MM or Z",
                syntax::time_offset(spec), loc, std::string(
                "Hint: valid  : +09:00, -05:30\n"
                "Hint: invalid: +9:00,  -5:30\n")));
        }
        return ok(value_t::local_datetime);
    }
    loc = first;

    if(syntax::local_date(spec).scan(loc).is_ok())
    {
        // bad time may appear after this.

        if( ! loc.eof())
        {
            const auto c = loc.current();
            if(c == 'T' || c == 't')
            {
                loc.advance();

                return err(make_syntax_error("bad time: must be HH:MM:SS.subsec",
                    syntax::local_time(spec), loc, std::string(
                    "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                    "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
            }
            if(c == ' ')
            {
                // A space is allowed as a delimiter between local time.
                // But there is a case where bad time follows a space.
                // - invalid: 2019-06-16 7:00:00
                // - valid  : 2019-06-16 07:00:00
                loc.advance();
                if( ! loc.eof() && ('0' <= loc.current() && loc.current() <= '9'))
                {
                    return err(make_syntax_error("bad time: must be HH:MM:SS.subsec",
                        syntax::local_time(spec), loc, std::string(
                        "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                        "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
                }
            }
            if('0' <= c && c <= '9')
            {
                return err(make_syntax_error("bad datetime: missing T or space",
                    character_either{'T', 't', ' '}, loc, std::string(
                    "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                    "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
            }
        }
        return ok(value_t::local_date);
    }
    loc = first;

    if(syntax::local_time(spec).scan(loc).is_ok())
    {
        return ok(value_t::local_time);
    }
    loc = first;

    if(syntax::floating(spec).scan(loc).is_ok())
    {
        if( ! loc.eof() && loc.current() == '_')
        {
            if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
            {
                return ok(value_t::floating);
            }
            auto src = source_location(region(loc));
            return err(make_error_info(
                "bad float: `_` must be surrounded by digits",
                std::move(src), "invalid underscore",
                "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n"));
        }
        return ok(value_t::floating);
    }
    loc = first;

    if(spec.ext_hex_float)
    {
        if(syntax::hex_floating(spec).scan(loc).is_ok())
        {
            if( ! loc.eof() && loc.current() == '_')
            {
                if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
                {
                    return ok(value_t::floating);
                }
                auto src = source_location(region(loc));
                return err(make_error_info(
                    "bad float: `_` must be surrounded by digits",
                    std::move(src), "invalid underscore",
                    "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                    "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n"));
            }
            return ok(value_t::floating);
        }
        loc = first;
    }

    if(auto int_reg = syntax::integer(spec).scan(loc))
    {
        if( ! loc.eof())
        {
            const auto c = loc.current();
            if(c == '_')
            {
                if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
                {
                    return ok(value_t::integer);
                }

                if(int_reg.length() <= 2 && (int_reg.as_string() == "0" ||
                    int_reg.as_string() == "-0" || int_reg.as_string() == "+0"))
                {
                    auto src = source_location(region(loc));
                    return err(make_error_info(
                        "bad integer: leading zero is not allowed in decimal int",
                        std::move(src), "leading zero",
                        "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                        "Hint: invalid: _42, 1__000, 0123\n"));
                }
                else
                {
                    auto src = source_location(region(loc));
                    return err(make_error_info(
                        "bad integer: `_` must be surrounded by digits",
                        std::move(src), "invalid underscore",
                        "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                        "Hint: invalid: _42, 1__000, 0123\n"));
                }
            }
            if('0' <= c && c <= '9')
            {
                if(loc.current() == '0')
                {
                    loc.retrace();
                    return err(make_error_info(
                        "bad integer: leading zero",
                        source_location(region(loc)), "leading zero is not allowed",
                        std::string("Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                                    "Hint: invalid: _42, 1__000, 0123\n")
                        ));
                }
                else // invalid digits, especially in oct/bin ints.
                {
                    return err(make_error_info(
                        "bad integer: invalid digit after an integer",
                        source_location(region(loc)), "this digit is not allowed",
                        std::string("Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                                    "Hint: invalid: _42, 1__000, 0123\n")
                        ));
                }
            }
            if(c == ':' || c == '-')
            {
                auto src = source_location(region(loc));
                return err(make_error_info("bad datetime: invalid format",
                    std::move(src), "here",
                    std::string("Hint: valid  : 1979-05-27T07:32:00-07:00, 1979-05-27 07:32:00.999999Z\n"
                                "Hint: invalid: 1979-05-27T7:32:00-7:00, 1979-05-27 7:32-00:30")
                    ));
            }
            if(c == '.' || c == 'e' || c == 'E')
            {
                auto src = source_location(region(loc));
                return err(make_error_info("bad float: invalid format",
                    std::move(src), "here", std::string(
                    "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                    "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n")));
            }
        }
        return ok(value_t::integer);
    }
    if( ! loc.eof() && loc.current() == '.')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("bad float: integer part is required before decimal point",
            std::move(src), "missing integer part", std::string(
            "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
            "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n")
            ));
    }
    if( ! loc.eof() && loc.current() == '_')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("bad number: `_` must be surrounded by digits",
            std::move(src), "digits required before `_`", std::string(
            "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
            "Hint: invalid: _42, 1__000, 0123\n")
            ));
    }

    auto src = source_location(region(loc));
    return err(make_error_info("bad format: unknown value appeared",
                std::move(src), "here"));
}

template<typename TC>
result<value_t, error_info>
guess_value_type(const location& loc, const context<TC>& ctx)
{
    const auto& sp = ctx.toml_spec();
    location inner(loc);

    switch(loc.current())
    {
        case '"' : {return ok(value_t::string);  }
        case '\'': {return ok(value_t::string);  }
        case '[' : {return ok(value_t::array);   }
        case '{' : {return ok(value_t::table);   }
        case 't' :
        {
            return ok(value_t::boolean);
        }
        case 'f' :
        {
            return ok(value_t::boolean);
        }
        case 'T' : // invalid boolean.
        {
            return err(make_syntax_error("toml::parse_value: "
                "`true` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::boolean(sp), inner));
        }
        case 'F' :
        {
            return err(make_syntax_error("toml::parse_value: "
                "`false` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::boolean(sp), inner));
        }
        case 'i' : // inf or string without quotes(syntax error).
        {
            if(literal("inf").scan(inner).is_ok())
            {
                return ok(value_t::floating);
            }
            else
            {
                return err(make_syntax_error("toml::parse_value: "
                    "`inf` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
        }
        case 'I' : // Inf or string without quotes(syntax error).
        {
            return err(make_syntax_error("toml::parse_value: "
                "`inf` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::floating(sp), inner));
        }
        case 'n' : // nan or null-extension
        {
            if(sp.ext_null_value)
            {
                if(literal("nan").scan(inner).is_ok())
                {
                    return ok(value_t::floating);
                }
                else if(literal("null").scan(inner).is_ok())
                {
                    return ok(value_t::empty);
                }
                else
                {
                    return err(make_syntax_error("toml::parse_value: "
                        "Both `nan` and `null` must be in lowercase. "
                        "A string must be surrounded by quotes.",
                        syntax::floating(sp), inner));
                }
            }
            else // must be nan.
            {
                if(literal("nan").scan(inner).is_ok())
                {
                    return ok(value_t::floating);
                }
                else
                {
                    return err(make_syntax_error("toml::parse_value: "
                        "`nan` must be in lowercase. "
                        "A string must be surrounded by quotes.",
                        syntax::floating(sp), inner));
                }
            }
        }
        case 'N' : // nan or null-extension
        {
            if(sp.ext_null_value)
            {
                return err(make_syntax_error("toml::parse_value: "
                    "Both `nan` and `null` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
            else
            {
                return err(make_syntax_error("toml::parse_value: "
                    "`nan` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
        }
        default  :
        {
            return guess_number_type(loc, ctx);
        }
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_value(location& loc, context<TC>& ctx)
{
    const auto ty_res = guess_value_type(loc, ctx);
    if(ty_res.is_err())
    {
        return err(ty_res.unwrap_err());
    }

    switch(ty_res.unwrap())
    {
        case value_t::empty:
        {
            if(ctx.toml_spec().ext_null_value)
            {
                return parse_null(loc, ctx);
            }
            else
            {
                auto src = source_location(region(loc));
                return err(make_error_info("toml::parse_value: unknown value appeared",
                            std::move(src), "here"));
            }
        }
        case value_t::boolean        : {return parse_boolean        (loc, ctx);}
        case value_t::integer        : {return parse_integer        (loc, ctx);}
        case value_t::floating       : {return parse_floating       (loc, ctx);}
        case value_t::string         : {return parse_string         (loc, ctx);}
        case value_t::offset_datetime: {return parse_offset_datetime(loc, ctx);}
        case value_t::local_datetime : {return parse_local_datetime (loc, ctx);}
        case value_t::local_date     : {return parse_local_date     (loc, ctx);}
        case value_t::local_time     : {return parse_local_time     (loc, ctx);}
        case value_t::array          : {return parse_array          (loc, ctx);}
        case value_t::table          : {return parse_inline_table   (loc, ctx);}
        default:
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_value: unknown value appeared",
                        std::move(src), "here"));
        }
    }
}

/* ============================================================================
 *  _____     _    _
 * |_   _|_ _| |__| |___
 *   | |/ _` | '_ \ / -_)
 *   |_|\__,_|_.__/_\___|
 */

template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_table_key(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::std_table(spec).scan(loc);
    if(!reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_table_key: invalid table key",
            syntax::std_table(spec), loc));
    }

    loc = first;
    loc.advance(); // skip [
    skip_whitespace(loc, ctx);

    auto keys_res = parse_key(loc, ctx);
    if(keys_res.is_err())
    {
        return err(std::move(keys_res.unwrap_err()));
    }

    skip_whitespace(loc, ctx);
    loc.advance(); // ]

    return ok(std::make_pair(std::move(keys_res.unwrap().first), std::move(reg)));
}

template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_array_table_key(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::array_table(spec).scan(loc);
    if(!reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_array_table_key: invalid array-of-tables key",
            syntax::array_table(spec), loc));
    }

    loc = first;
    loc.advance(); // [
    loc.advance(); // [
    skip_whitespace(loc, ctx);

    auto keys_res = parse_key(loc, ctx);
    if(keys_res.is_err())
    {
        return err(std::move(keys_res.unwrap_err()));
    }

    skip_whitespace(loc, ctx);
    loc.advance(); // ]
    loc.advance(); // ]

    return ok(std::make_pair(std::move(keys_res.unwrap().first), std::move(reg)));
}

// called after reading [table.keys] and comments around it.
// Since table may already contain a subtable ([x.y.z] can be defined before [x]),
// the table that is being parsed is passed as an argument.
template<typename TC>
result<none_t, error_info>
parse_table(location& loc, context<TC>& ctx, basic_value<TC>& table)
{
    assert(table.is_table());

    const auto num_errors = ctx.errors().size();
    const auto& spec = ctx.toml_spec();

    // clear indent info
    table.as_table_fmt().indent_type = indent_char::none;

    bool newline_found = true;
    while( ! loc.eof())
    {
        const auto start = loc;

        auto sp = skip_multiline_spacer(loc, ctx, newline_found);

        // if reached to EOF, the table ends here. return.
        if(loc.eof())
        {
            break;
        }
        // if next table is comming, return.
        if(sequence(syntax::ws(spec), character('[')).scan(loc).is_ok())
        {
            loc = start;
            break;
        }
        // otherwise, it should be a key-value pair.
        newline_found = newline_found || (sp.has_value() && sp.value().newline_found);
        if( ! newline_found)
        {
            return err(make_error_info("toml::parse_table: "
                "newline (LF / CRLF) or EOF is expected",
                source_location(region(loc)), "here"));
        }
        if(sp.has_value() && sp.value().indent_type != indent_char::none)
        {
            table.as_table_fmt().indent_type = sp.value().indent_type;
            table.as_table_fmt().body_indent = sp.value().indent;
        }

        newline_found = false; // reset
        if(auto kv_res = parse_key_value_pair(loc, ctx))
        {
            auto keys    = std::move(kv_res.unwrap().first.first);
            auto key_reg = std::move(kv_res.unwrap().first.second);
            auto val     = std::move(kv_res.unwrap().second);

            if(sp.has_value())
            {
                for(const auto& com : sp.value().comments)
                {
                    val.comments().push_back(com);
                }
            }

            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    val.comments().push_back(com_opt.value());
                    newline_found = true; // comment includes newline at the end
                }
            }
            else
            {
                ctx.report_error(std::move(com_res.unwrap_err()));
            }

            auto ins_res = insert_value(inserting_value_kind::dotted_keys,
                    std::addressof(table.as_table()),
                    keys, std::move(key_reg), std::move(val));
            if(ins_res.is_err())
            {
                ctx.report_error(std::move(ins_res.unwrap_err()));
            }
        }
        else
        {
            ctx.report_error(std::move(kv_res.unwrap_err()));
            skip_key_value_pair(loc, ctx);
        }
    }

    if(num_errors < ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.pop_last_error());
    }
    return ok();
}

template<typename TC>
result<basic_value<TC>, std::vector<error_info>>
parse_file(location& loc, context<TC>& ctx)
{
    using value_type = basic_value<TC>;
    using table_type = typename value_type::table_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    if(loc.eof())
    {
        return ok(value_type(table_type(), table_format_info{}, {}, region(loc)));
    }

    value_type root(table_type(), table_format_info{}, {}, region(loc));
    root.as_table_fmt().fmt = table_format::multiline;
    root.as_table_fmt().indent_type = indent_char::none;

    // parse top comment.
    //
    // ```toml
    // # this is a comment for the top-level table.
    //
    // key = "the first value"
    // ```
    //
    // ```toml
    // # this is a comment for "the first value".
    // key = "the first value"
    // ```
    while( ! loc.eof())
    {
        if(auto com_res = parse_comment_line(loc, ctx))
        {
            if(auto com_opt = com_res.unwrap())
            {
                root.comments().push_back(std::move(com_opt.value()));
            }
            else // no comment found.
            {
                // if it is not an empty line, clear the root comment.
                if( ! sequence(syntax::ws(spec), syntax::newline(spec)).scan(loc).is_ok())
                {
                    loc = first;
                    root.comments().clear();
                }
                break;
            }
        }
        else
        {
            ctx.report_error(std::move(com_res.unwrap_err()));
            skip_comment_block(loc, ctx);
        }
    }

    // parse root table
    {
        const auto res = parse_table(loc, ctx, root);
        if(res.is_err())
        {
            ctx.report_error(std::move(res.unwrap_err()));
            skip_until_next_table(loc, ctx);
        }
    }

    // parse tables

    while( ! loc.eof())
    {
        auto sp = skip_multiline_spacer(loc, ctx, /*newline_found=*/true);

        if(auto key_res = parse_array_table_key(loc, ctx))
        {
            auto key = std::move(std::get<0>(key_res.unwrap()));
            auto reg = std::move(std::get<1>(key_res.unwrap()));

            std::vector<std::string> com;
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    com.push_back(std::move(sp.value().comments.at(i)));
                }
            }

            // [table.def] must be followed by one of
            // - a comment line
            // - whitespace + newline
            // - EOF
            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    com.push_back(com_opt.value());
                }
                else // if there is no comment, ws+newline must exist (or EOF)
                {
                    skip_whitespace(loc, ctx);
                    if( ! loc.eof() && ! syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
                    {
                        ctx.report_error(make_syntax_error("toml::parse_file: "
                            "newline (or EOF) expected",
                            syntax::newline(ctx.toml_spec()), loc));
                        skip_until_next_table(loc, ctx);
                        continue;
                    }
                }
            }
            else // comment syntax error (rare)
            {
                ctx.report_error(com_res.unwrap_err());
                skip_until_next_table(loc, ctx);
                continue;
            }

            table_format_info fmt;
            fmt.fmt = table_format::multiline;
            fmt.indent_type = indent_char::none;
            auto tab = value_type(table_type{}, std::move(fmt), std::move(com), reg);

            auto inserted = insert_value(inserting_value_kind::array_table,
                std::addressof(root.as_table()),
                key, std::move(reg), std::move(tab));

            if(inserted.is_err())
            {
                ctx.report_error(inserted.unwrap_err());

                // check errors in the table
                auto tmp = basic_value<TC>(table_type());
                auto res = parse_table(loc, ctx, tmp);
                if(res.is_err())
                {
                    ctx.report_error(res.unwrap_err());
                    skip_until_next_table(loc, ctx);
                }
                continue;
            }

            auto tab_ptr = inserted.unwrap();
            assert(tab_ptr);

            const auto tab_res = parse_table(loc, ctx, *tab_ptr);
            if(tab_res.is_err())
            {
                ctx.report_error(tab_res.unwrap_err());
                skip_until_next_table(loc, ctx);
            }

            // parse_table first clears `indent_type`.
            // to keep header indent info, we must store it later.
            if(sp.has_value() && sp.value().indent_type != indent_char::none)
            {
                tab_ptr->as_table_fmt().indent_type = sp.value().indent_type;
                tab_ptr->as_table_fmt().name_indent = sp.value().indent;
            }
            continue;
        }
        if(auto key_res = parse_table_key(loc, ctx))
        {
            auto key = std::move(std::get<0>(key_res.unwrap()));
            auto reg = std::move(std::get<1>(key_res.unwrap()));

            std::vector<std::string> com;
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    com.push_back(std::move(sp.value().comments.at(i)));
                }
            }

            // [table.def] must be followed by one of
            // - a comment line
            // - whitespace + newline
            // - EOF
            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    com.push_back(com_opt.value());
                }
                else // if there is no comment, ws+newline must exist (or EOF)
                {
                    skip_whitespace(loc, ctx);
                    if( ! loc.eof() && ! syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
                    {
                        ctx.report_error(make_syntax_error("toml::parse_file: "
                            "newline (or EOF) expected",
                            syntax::newline(ctx.toml_spec()), loc));
                        skip_until_next_table(loc, ctx);
                        continue;
                    }
                }
            }
            else // comment syntax error (rare)
            {
                ctx.report_error(com_res.unwrap_err());
                skip_until_next_table(loc, ctx);
                continue;
            }

            table_format_info fmt;
            fmt.fmt = table_format::multiline;
            fmt.indent_type = indent_char::none;
            auto tab = value_type(table_type{}, std::move(fmt), std::move(com), reg);

            auto inserted = insert_value(inserting_value_kind::std_table,
                std::addressof(root.as_table()),
                key, std::move(reg), std::move(tab));

            if(inserted.is_err())
            {
                ctx.report_error(inserted.unwrap_err());

                // check errors in the table
                auto tmp = basic_value<TC>(table_type());
                auto res = parse_table(loc, ctx, tmp);
                if(res.is_err())
                {
                    ctx.report_error(res.unwrap_err());
                    skip_until_next_table(loc, ctx);
                }
                continue;
            }

            auto tab_ptr = inserted.unwrap();
            assert(tab_ptr);

            const auto tab_res = parse_table(loc, ctx, *tab_ptr);
            if(tab_res.is_err())
            {
                ctx.report_error(tab_res.unwrap_err());
                skip_until_next_table(loc, ctx);
            }
            if(sp.has_value() && sp.value().indent_type != indent_char::none)
            {
                tab_ptr->as_table_fmt().indent_type = sp.value().indent_type;
                tab_ptr->as_table_fmt().name_indent = sp.value().indent;
            }
            continue;
        }

        // does not match array_table nor std_table. report an error.
        const auto keytop = loc;
        const auto maybe_array_of_tables = literal("[[").scan(loc).is_ok();
        loc = keytop;

        if(maybe_array_of_tables)
        {
            ctx.report_error(make_syntax_error("toml::parse_file: invalid array-table key",
                syntax::array_table(spec), loc));
        }
        else
        {
            ctx.report_error(make_syntax_error("toml::parse_file: invalid table key",
                syntax::std_table(spec), loc));
        }
        skip_until_next_table(loc, ctx);
    }

    if( ! ctx.errors().empty())
    {
        return err(std::move(ctx.errors()));
    }
    return ok(std::move(root));
}

template<typename TC>
result<basic_value<TC>, std::vector<error_info>>
parse_impl(std::vector<location::char_type> cs, std::string fname, const spec& s)
{
    using value_type = basic_value<TC>;
    using table_type = typename value_type::table_type;

    // an empty file is a valid toml file.
    if(cs.empty())
    {
        auto src = std::make_shared<std::vector<location::char_type>>(std::move(cs));
        location loc(std::move(src), std::move(fname));
        return ok(value_type(table_type(), table_format_info{}, std::vector<std::string>{}, region(loc)));
    }

    // to simplify parser, add newline at the end if there is no LF.
    // But, if it has raw CR, the file is invalid (in TOML, CR is not a valid
    // newline char). if it ends with CR, do not add LF and report it.
    if(cs.back() != '\n' && cs.back() != '\r')
    {
        cs.push_back('\n');
    }

    auto src = std::make_shared<std::vector<location::char_type>>(std::move(cs));

    location loc(std::move(src), std::move(fname));

    // skip BOM if found
    if(loc.source()->size() >= 3)
    {
        auto first = loc.get_location();

        const auto c0 = loc.current(); loc.advance();
        const auto c1 = loc.current(); loc.advance();
        const auto c2 = loc.current(); loc.advance();

        const auto bom_found = (c0 == 0xEF) && (c1 == 0xBB) && (c2 == 0xBF);
        if( ! bom_found)
        {
            loc.set_location(first);
        }
    }

    context<TC> ctx(s);

    return parse_file(loc, ctx);
}

} // detail

// -----------------------------------------------------------------------------
// parse(byte array)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::vector<unsigned char> content, std::string filename,
          spec s = spec::default_version())
{
    return detail::parse_impl<TC>(std::move(content), std::move(filename), std::move(s));
}
template<typename TC = type_config>
basic_value<TC>
parse(std::vector<unsigned char> content, std::string filename,
      spec s = spec::default_version())
{
    auto res = try_parse<TC>(std::move(content), std::move(filename), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// -----------------------------------------------------------------------------
// parse(istream)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::istream& is, std::string fname = "unknown file", spec s = spec::default_version())
{
    const auto beg = is.tellg();
    is.seekg(0, std::ios::end);
    const auto end = is.tellg();
    const auto fsize = end - beg;
    is.seekg(beg);

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize), '\0');
    is.read(reinterpret_cast<char*>(letters.data()), static_cast<std::streamsize>(fsize));

    return detail::parse_impl<TC>(std::move(letters), std::move(fname), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse(std::istream& is, std::string fname = "unknown file", spec s = spec::default_version())
{
    auto res = try_parse<TC>(is, std::move(fname), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// -----------------------------------------------------------------------------
// parse(filename)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::string fname, spec s = spec::default_version())
{
    std::ifstream ifs(fname, std::ios_base::binary);
    if(!ifs.good())
    {
        std::vector<error_info> e;
        e.push_back(error_info("toml::parse: Error opening file \"" + fname + "\"", {}));
        return err(std::move(e));
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return try_parse<TC>(ifs, std::move(fname), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse(std::string fname, spec s = spec::default_version())
{
    std::ifstream ifs(fname, std::ios_base::binary);
    if(!ifs.good())
    {
        throw file_io_error("toml::parse: error opening file", fname);
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return parse<TC>(ifs, std::move(fname), std::move(s));
}

template<typename TC = type_config, std::size_t N>
result<basic_value<TC>, std::vector<error_info>>
try_parse(const char (&fname)[N], spec s = spec::default_version())
{
    return try_parse<TC>(std::string(fname), std::move(s));
}

template<typename TC = type_config, std::size_t N>
basic_value<TC> parse(const char (&fname)[N], spec s = spec::default_version())
{
    return parse<TC>(std::string(fname), std::move(s));
}

// ----------------------------------------------------------------------------
// parse_str

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse_str(std::string content, spec s = spec::default_version(),
              cxx::source_location loc = cxx::source_location::current())
{
    std::istringstream iss(std::move(content));
    std::string name("internal string" + cxx::to_string(loc));
    return try_parse<TC>(iss, std::move(name), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse_str(std::string content, spec s = spec::default_version(),
        cxx::source_location loc = cxx::source_location::current())
{
    auto res = try_parse_str<TC>(std::move(content), std::move(s), std::move(loc));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// ----------------------------------------------------------------------------
// filesystem

#if defined(TOML11_HAS_FILESYSTEM)

template<typename TC = type_config, typename FSPATH>
cxx::enable_if_t<std::is_same<FSPATH, std::filesystem::path>::value,
    result<basic_value<TC>, std::vector<error_info>>>
try_parse(const FSPATH& fpath, spec s = spec::default_version())
{
    std::ifstream ifs(fpath, std::ios_base::binary);
    if(!ifs.good())
    {
        std::vector<error_info> e;
        e.push_back(error_info("toml::parse: Error opening file \"" + fpath.string() + "\"", {}));
        return err(std::move(e));
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return try_parse<TC>(ifs, fpath.string(), std::move(s));
}

template<typename TC = type_config, typename FSPATH>
cxx::enable_if_t<std::is_same<FSPATH, std::filesystem::path>::value,
    basic_value<TC>>
parse(const FSPATH& fpath, spec s = spec::default_version())
{
    std::ifstream ifs(fpath, std::ios_base::binary);
    if(!ifs.good())
    {
        throw file_io_error("toml::parse: error opening file", fpath.string());
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return parse<TC>(ifs, fpath.string(), std::move(s));
}
#endif

// -----------------------------------------------------------------------------
// FILE*

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(FILE* fp, std::string filename, spec s = spec::default_version())
{
    const long beg = std::ftell(fp);
    if (beg == -1L)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to access: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const int res_seekend = std::fseek(fp, 0, SEEK_END);
    if (res_seekend != 0)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to seek: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const long end = std::ftell(fp);
    if (end == -1L)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to access: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const auto fsize = end - beg;

    const auto res_seekbeg = std::fseek(fp, beg, SEEK_SET);
    if (res_seekbeg != 0)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to seek: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});

    }

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize));
    const auto actual = std::fread(letters.data(), sizeof(char), static_cast<std::size_t>(fsize), fp);
    if(actual != static_cast<std::size_t>(fsize))
    {
        return err(std::vector<error_info>{error_info(
            std::string("File size changed: \"") + filename +
            std::string("\" make sure that FILE* is in binary mode "
                        "to avoid LF <-> CRLF conversion"), {}
        )});
    }

    return detail::parse_impl<TC>(std::move(letters), std::move(filename), std::move(s));
}

template<typename TC = type_config>
basic_value<TC>
parse(FILE* fp, std::string filename, spec s = spec::default_version())
{
    const long beg = std::ftell(fp);
    if (beg == -1L)
    {
        throw file_io_error(errno, "Failed to access", filename);
    }

    const int res_seekend = std::fseek(fp, 0, SEEK_END);
    if (res_seekend != 0)
    {
        throw file_io_error(errno, "Failed to seek", filename);
    }

    const long end = std::ftell(fp);
    if (end == -1L)
    {
        throw file_io_error(errno, "Failed to access", filename);
    }

    const auto fsize = end - beg;

    const auto res_seekbeg = std::fseek(fp, beg, SEEK_SET);
    if (res_seekbeg != 0)
    {
        throw file_io_error(errno, "Failed to seek", filename);
    }

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize));
    const auto actual = std::fread(letters.data(), sizeof(char), static_cast<std::size_t>(fsize), fp);
    if(actual != static_cast<std::size_t>(fsize))
    {
        throw file_io_error(errno, "File size changed; make sure that "
            "FILE* is in binary mode to avoid LF <-> CRLF conversion", filename);
    }

    auto res = detail::parse_impl<TC>(std::move(letters), std::move(filename), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
struct type_config;
struct ordered_type_config;

extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::vector<unsigned char>, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::istream&, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(FILE*, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse_str<type_config>(std::string, spec, cxx::source_location);

extern template basic_value<type_config> parse<type_config>(std::vector<unsigned char>, std::string, spec);
extern template basic_value<type_config> parse<type_config>(std::istream&, std::string, spec);
extern template basic_value<type_config> parse<type_config>(std::string, spec);
extern template basic_value<type_config> parse<type_config>(FILE*, std::string, spec);
extern template basic_value<type_config> parse_str<type_config>(std::string, spec, cxx::source_location);

extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::vector<unsigned char>, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::istream&, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(FILE*, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse_str<ordered_type_config>(std::string, spec, cxx::source_location);

extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::vector<unsigned char>, std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::istream&, std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(FILE*, std::string, spec);
extern template basic_value<ordered_type_config> parse_str<ordered_type_config>(std::string, spec, cxx::source_location);

#if defined(TOML11_HAS_FILESYSTEM)
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, result<basic_value<type_config>,         std::vector<error_info>>> try_parse<type_config,         std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, result<basic_value<ordered_type_config>, std::vector<error_info>>> try_parse<ordered_type_config, std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, basic_value<type_config>                                         > parse    <type_config,         std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, basic_value<ordered_type_config>                                 > parse    <ordered_type_config, std::filesystem::path>(const std::filesystem::path&, spec);
#endif // filesystem

} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_PARSER_HPP
