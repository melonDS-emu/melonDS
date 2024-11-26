#ifndef TOML11_TOML_FWD_HPP
#define TOML11_TOML_FWD_HPP

// IWYU pragma: begin_exports
#include "toml11/version.hpp"
#include "toml11/compat.hpp"
#include "toml11/conversion.hpp"
#include "toml11/from.hpp"
#include "toml11/into.hpp"
// IWYU pragma: end_exports

#include <cstdint>

#ifdef TOML11_COLORIZE_ERROR_MESSAGE
#define TOML11_ERROR_MESSAGE_COLORIZED true
#else
#define TOML11_ERROR_MESSAGE_COLORIZED false
#endif

namespace toml
{
class discard_comments;
class preserve_comments;

enum class month_t : std::uint8_t;
struct local_date;
struct local_time;
struct time_offset;
struct local_datetime;
struct offset_datetime;

struct error_info;

struct exception;

enum class indent_char : std::uint8_t;
enum class integer_format : std::uint8_t;
enum class floating_format : std::uint8_t;
enum class string_format : std::uint8_t;
enum class datetime_delimiter_kind : std::uint8_t;
enum class array_format : std::uint8_t;
enum class table_format : std::uint8_t;

struct boolean_format_info;
struct integer_format_info;
struct floating_format_info;
struct string_format_info;
struct offset_datetime_format_info;
struct local_datetime_format_info;
struct local_date_format_info;
struct local_time_format_info;
struct array_format_info;
struct table_format_info;

template<typename Key, typename Val, typename Cmp, typename Allocator>
class ordered_map;

struct syntax_error;
struct file_io_error;

struct bad_result_access;
template<typename T>
struct success;
template<typename T>
struct failure;
template<typename T, typename E>
struct result;

struct source_location;

struct semantic_version;
struct spec;


template<typename TypeConfig>
class basic_value;
struct type_error;

struct type_config;
using value = basic_value<type_config>;

struct ordered_type_config;
using ordered_value = basic_value<ordered_type_config>;

enum class value_t : std::uint8_t;

} // toml
#endif// TOML11_TOML_HPP
