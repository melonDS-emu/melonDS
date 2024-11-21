#ifndef TOML11_FORMAT_FWD_HPP
#define TOML11_FORMAT_FWD_HPP

#include <iosfwd>
#include <string>
#include <utility>

#include <cstddef>
#include <cstdint>

namespace toml
{

// toml types with serialization info

enum class indent_char : std::uint8_t
{
    space, // use space
    tab,   // use tab
    none   // no indent
};

std::ostream& operator<<(std::ostream& os, const indent_char& c);
std::string to_string(const indent_char c);

// ----------------------------------------------------------------------------
// boolean

struct boolean_format_info
{
    // nothing, for now
};

inline bool operator==(const boolean_format_info&, const boolean_format_info&) noexcept
{
    return true;
}
inline bool operator!=(const boolean_format_info&, const boolean_format_info&) noexcept
{
    return false;
}

// ----------------------------------------------------------------------------
// integer

enum class integer_format : std::uint8_t
{
    dec = 0,
    bin = 1,
    oct = 2,
    hex = 3,
};

std::ostream& operator<<(std::ostream& os, const integer_format f);
std::string to_string(const integer_format);

struct integer_format_info
{
    integer_format fmt = integer_format::dec;
    bool        uppercase = true; // hex with uppercase
    std::size_t width     = 0;    // minimal width (may exceed)
    std::size_t spacer    = 0;    // position of `_` (if 0, no spacer)
    std::string suffix    = "";   // _suffix (library extension)
};

bool operator==(const integer_format_info&, const integer_format_info&) noexcept;
bool operator!=(const integer_format_info&, const integer_format_info&) noexcept;

// ----------------------------------------------------------------------------
// floating

enum class floating_format : std::uint8_t
{
    defaultfloat = 0,
    fixed        = 1, // does not include exponential part
    scientific   = 2, // always include exponential part
    hex          = 3  // hexfloat extension
};

std::ostream& operator<<(std::ostream& os, const floating_format f);
std::string to_string(const floating_format);

struct floating_format_info
{
    floating_format fmt = floating_format::defaultfloat;
    std::size_t prec  = 0;        // precision (if 0, use the default)
    std::string suffix = "";      // 1.0e+2_suffix (library extension)
};

bool operator==(const floating_format_info&, const floating_format_info&) noexcept;
bool operator!=(const floating_format_info&, const floating_format_info&) noexcept;

// ----------------------------------------------------------------------------
// string

enum class string_format : std::uint8_t
{
    basic             = 0,
    literal           = 1,
    multiline_basic   = 2,
    multiline_literal = 3
};

std::ostream& operator<<(std::ostream& os, const string_format f);
std::string to_string(const string_format);

struct string_format_info
{
    string_format fmt = string_format::basic;
    bool start_with_newline    = false;
};

bool operator==(const string_format_info&, const string_format_info&) noexcept;
bool operator!=(const string_format_info&, const string_format_info&) noexcept;

// ----------------------------------------------------------------------------
// datetime

enum class datetime_delimiter_kind : std::uint8_t
{
    upper_T = 0,
    lower_t = 1,
    space   = 2,
};
std::ostream& operator<<(std::ostream& os, const datetime_delimiter_kind d);
std::string to_string(const datetime_delimiter_kind);

struct offset_datetime_format_info
{
    datetime_delimiter_kind delimiter = datetime_delimiter_kind::upper_T;
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const offset_datetime_format_info&, const offset_datetime_format_info&) noexcept;
bool operator!=(const offset_datetime_format_info&, const offset_datetime_format_info&) noexcept;

struct local_datetime_format_info
{
    datetime_delimiter_kind delimiter = datetime_delimiter_kind::upper_T;
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const local_datetime_format_info&, const local_datetime_format_info&) noexcept;
bool operator!=(const local_datetime_format_info&, const local_datetime_format_info&) noexcept;

struct local_date_format_info
{
    // nothing, for now
};

bool operator==(const local_date_format_info&, const local_date_format_info&) noexcept;
bool operator!=(const local_date_format_info&, const local_date_format_info&) noexcept;

struct local_time_format_info
{
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const local_time_format_info&, const local_time_format_info&) noexcept;
bool operator!=(const local_time_format_info&, const local_time_format_info&) noexcept;

// ----------------------------------------------------------------------------
// array

enum class array_format : std::uint8_t
{
    default_format  = 0,
    oneline         = 1,
    multiline       = 2,
    array_of_tables = 3 // [[format.in.this.way]]
};

std::ostream& operator<<(std::ostream& os, const array_format f);
std::string to_string(const array_format);

struct array_format_info
{
    array_format fmt            = array_format::default_format;
    indent_char  indent_type    = indent_char::space;
    std::int32_t body_indent    = 4; // indent in case of multiline
    std::int32_t closing_indent = 0; // indent of `]`
};

bool operator==(const array_format_info&, const array_format_info&) noexcept;
bool operator!=(const array_format_info&, const array_format_info&) noexcept;

// ----------------------------------------------------------------------------
// table

enum class table_format : std::uint8_t
{
    multiline         = 0, // [foo] \n bar = "baz"
    oneline           = 1, // foo = {bar = "baz"}
    dotted            = 2, // foo.bar = "baz"
    multiline_oneline = 3, // foo = { \n bar = "baz" \n }
    implicit          = 4  // [x] defined by [x.y.z]. skip in serializer.
};

std::ostream& operator<<(std::ostream& os, const table_format f);
std::string to_string(const table_format);

struct table_format_info
{
    table_format fmt = table_format::multiline;
    indent_char  indent_type    = indent_char::space;
    std::int32_t body_indent    = 0; // indent of values
    std::int32_t name_indent    = 0; // indent of [table]
    std::int32_t closing_indent = 0; // in case of {inline-table}
};

bool operator==(const table_format_info&, const table_format_info&) noexcept;
bool operator!=(const table_format_info&, const table_format_info&) noexcept;

// ----------------------------------------------------------------------------
// wrapper

namespace detail
{
template<typename T, typename F>
struct value_with_format
{
    using value_type  = T;
    using format_type = F;

    value_with_format()  = default;
    ~value_with_format() = default;
    value_with_format(const value_with_format&) = default;
    value_with_format(value_with_format&&)      = default;
    value_with_format& operator=(const value_with_format&) = default;
    value_with_format& operator=(value_with_format&&)      = default;

    value_with_format(value_type v, format_type f)
        : value{std::move(v)}, format{std::move(f)}
    {}

    template<typename U>
    value_with_format(value_with_format<U, format_type> other)
        : value{std::move(other.value)}, format{std::move(other.format)}
    {}

    value_type  value;
    format_type format;
};
} // detail

} // namespace toml
#endif // TOML11_FORMAT_FWD_HPP
