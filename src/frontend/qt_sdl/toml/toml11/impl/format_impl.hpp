#ifndef TOML11_FORMAT_IMPL_HPP
#define TOML11_FORMAT_IMPL_HPP

#include "../fwd/format_fwd.hpp"
#include "../version.hpp"

#include <ostream>
#include <sstream>

namespace toml
{

// toml types with serialization info

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const indent_char& c)
{
    switch(c)
    {
        case indent_char::space:         {os << "space"        ; break;}
        case indent_char::tab:           {os << "tab"          ; break;}
        case indent_char::none:          {os << "none"         ; break;}
        default:
        {
            os << "unknown indent char: " << static_cast<std::uint8_t>(c);
        }
    }
    return os;
}

TOML11_INLINE std::string to_string(const indent_char c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

// ----------------------------------------------------------------------------
// boolean

// ----------------------------------------------------------------------------
// integer

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const integer_format f)
{
    switch(f)
    {
        case integer_format::dec: {os << "dec"; break;}
        case integer_format::bin: {os << "bin"; break;}
        case integer_format::oct: {os << "oct"; break;}
        case integer_format::hex: {os << "hex"; break;}
        default:
        {
            os << "unknown integer_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const integer_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}


TOML11_INLINE bool operator==(const integer_format_info& lhs, const integer_format_info& rhs) noexcept
{
    return lhs.fmt       == rhs.fmt    &&
           lhs.uppercase == rhs.uppercase &&
           lhs.width     == rhs.width  &&
           lhs.spacer    == rhs.spacer &&
           lhs.suffix    == rhs.suffix ;
}
TOML11_INLINE bool operator!=(const integer_format_info& lhs, const integer_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// floating

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const floating_format f)
{
    switch(f)
    {
        case floating_format::defaultfloat: {os << "defaultfloat"; break;}
        case floating_format::fixed       : {os << "fixed"       ; break;}
        case floating_format::scientific  : {os << "scientific"  ; break;}
        case floating_format::hex         : {os << "hex"         ; break;}
        default:
        {
            os << "unknown floating_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const floating_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const floating_format_info& lhs, const floating_format_info& rhs) noexcept
{
    return lhs.fmt          == rhs.fmt          &&
           lhs.prec         == rhs.prec         &&
           lhs.suffix       == rhs.suffix       ;
}
TOML11_INLINE bool operator!=(const floating_format_info& lhs, const floating_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// string

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const string_format f)
{
    switch(f)
    {
        case string_format::basic            : {os << "basic"            ; break;}
        case string_format::literal          : {os << "literal"          ; break;}
        case string_format::multiline_basic  : {os << "multiline_basic"  ; break;}
        case string_format::multiline_literal: {os << "multiline_literal"; break;}
        default:
        {
            os << "unknown string_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const string_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const string_format_info& lhs, const string_format_info& rhs) noexcept
{
    return lhs.fmt                == rhs.fmt                &&
           lhs.start_with_newline == rhs.start_with_newline ;
}
TOML11_INLINE bool operator!=(const string_format_info& lhs, const string_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}
// ----------------------------------------------------------------------------
// datetime

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const datetime_delimiter_kind d)
{
    switch(d)
    {
        case datetime_delimiter_kind::upper_T: { os << "upper_T, "; break; }
        case datetime_delimiter_kind::lower_t: { os << "lower_t, "; break; }
        case datetime_delimiter_kind::space:   { os << "space, ";   break; }
        default:
        {
            os << "unknown datetime delimiter: " << static_cast<std::uint8_t>(d);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const datetime_delimiter_kind c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const offset_datetime_format_info& lhs, const offset_datetime_format_info& rhs) noexcept
{
    return lhs.delimiter           == rhs.delimiter   &&
           lhs.has_seconds         == rhs.has_seconds &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const offset_datetime_format_info& lhs, const offset_datetime_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_datetime_format_info& lhs, const local_datetime_format_info& rhs) noexcept
{
    return lhs.delimiter           == rhs.delimiter   &&
           lhs.has_seconds         == rhs.has_seconds &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const local_datetime_format_info& lhs, const local_datetime_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_date_format_info&, const local_date_format_info&) noexcept
{
    return true;
}
TOML11_INLINE bool operator!=(const local_date_format_info& lhs, const local_date_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_time_format_info& lhs, const local_time_format_info& rhs) noexcept
{
    return lhs.has_seconds         == rhs.has_seconds         &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const local_time_format_info& lhs, const local_time_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// array

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const array_format f)
{
    switch(f)
    {
        case array_format::default_format : {os << "default_format" ; break;}
        case array_format::oneline        : {os << "oneline"        ; break;}
        case array_format::multiline      : {os << "multiline"      ; break;}
        case array_format::array_of_tables: {os << "array_of_tables"; break;}
        default:
        {
            os << "unknown array_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const array_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const array_format_info& lhs, const array_format_info& rhs) noexcept
{
    return lhs.fmt            == rhs.fmt            &&
           lhs.indent_type    == rhs.indent_type    &&
           lhs.body_indent    == rhs.body_indent    &&
           lhs.closing_indent == rhs.closing_indent ;
}
TOML11_INLINE bool operator!=(const array_format_info& lhs, const array_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// table

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const table_format f)
{
    switch(f)
    {
        case table_format::multiline        : {os << "multiline"        ; break;}
        case table_format::oneline          : {os << "oneline"          ; break;}
        case table_format::dotted           : {os << "dotted"           ; break;}
        case table_format::multiline_oneline: {os << "multiline_oneline"; break;}
        case table_format::implicit         : {os << "implicit"         ; break;}
        default:
        {
            os << "unknown table_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const table_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const table_format_info& lhs, const table_format_info& rhs) noexcept
{
    return lhs.fmt                == rhs.fmt              &&
           lhs.indent_type        == rhs.indent_type      &&
           lhs.body_indent        == rhs.body_indent      &&
           lhs.name_indent        == rhs.name_indent      &&
           lhs.closing_indent     == rhs.closing_indent   ;
}
TOML11_INLINE bool operator!=(const table_format_info& lhs, const table_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

} // namespace toml
#endif // TOML11_FORMAT_IMPL_HPP
