#ifndef TOML11_DATETIME_IMPL_HPP
#define TOML11_DATETIME_IMPL_HPP

#include "../fwd/datetime_fwd.hpp"
#include "../version.hpp"

#include <array>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <tuple>

#include <cstdlib>
#include <ctime>

namespace toml
{

// To avoid non-threadsafe std::localtime. In C11 (not C++11!), localtime_s is
// provided in the absolutely same purpose, but C++11 is actually not compatible
// with C11. We need to dispatch the function depending on the OS.
namespace detail
{
// TODO: find more sophisticated way to handle this
#if defined(_MSC_VER)
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::localtime_s(&dst, src);
    if (result) { throw std::runtime_error("localtime_s failed."); }
    return dst;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::gmtime_s(&dst, src);
    if (result) { throw std::runtime_error("gmtime_s failed."); }
    return dst;
}
#elif (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1) || defined(_XOPEN_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_POSIX_SOURCE)
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::localtime_r(src, &dst);
    if (!result) { throw std::runtime_error("localtime_r failed."); }
    return dst;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::gmtime_r(src, &dst);
    if (!result) { throw std::runtime_error("gmtime_r failed."); }
    return dst;
}
#else // fallback. not threadsafe
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    const auto result = std::localtime(src);
    if (!result) { throw std::runtime_error("localtime failed."); }
    return *result;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    const auto result = std::gmtime(src);
    if (!result) { throw std::runtime_error("gmtime failed."); }
    return *result;
}
#endif
} // detail

// ----------------------------------------------------------------------------

TOML11_INLINE local_date::local_date(const std::chrono::system_clock::time_point& tp)
{
    const auto t    = std::chrono::system_clock::to_time_t(tp);
    const auto time = detail::localtime_s(&t);
    *this = local_date(time);
}

TOML11_INLINE local_date::local_date(const std::time_t t)
    : local_date{std::chrono::system_clock::from_time_t(t)}
{}

TOML11_INLINE local_date::operator std::chrono::system_clock::time_point() const
{
    // std::mktime returns date as local time zone. no conversion needed
    std::tm t;
    t.tm_sec   = 0;
    t.tm_min   = 0;
    t.tm_hour  = 0;
    t.tm_mday  = static_cast<int>(this->day);
    t.tm_mon   = static_cast<int>(this->month);
    t.tm_year  = static_cast<int>(this->year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

TOML11_INLINE local_date::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE bool operator==(const local_date& lhs, const local_date& rhs)
{
    return std::make_tuple(lhs.year, lhs.month, lhs.day) ==
           std::make_tuple(rhs.year, rhs.month, rhs.day);
}
TOML11_INLINE bool operator!=(const local_date& lhs, const local_date& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_date& lhs, const local_date& rhs)
{
    return std::make_tuple(lhs.year, lhs.month, lhs.day) <
           std::make_tuple(rhs.year, rhs.month, rhs.day);
}
TOML11_INLINE bool operator<=(const local_date& lhs, const local_date& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_date& lhs, const local_date& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_date& lhs, const local_date& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_date& date)
{
    os << std::setfill('0') << std::setw(4) << static_cast<int>(date.year )     << '-';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(date.month) + 1 << '-';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(date.day  )    ;
    return os;
}

TOML11_INLINE std::string to_string(const local_date& date)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << date;
    return oss.str();
}

// -----------------------------------------------------------------------------

TOML11_INLINE local_time::operator std::chrono::nanoseconds() const
{
    return std::chrono::nanoseconds (this->nanosecond)  +
           std::chrono::microseconds(this->microsecond) +
           std::chrono::milliseconds(this->millisecond) +
           std::chrono::seconds(this->second) +
           std::chrono::minutes(this->minute) +
           std::chrono::hours(this->hour);
}

TOML11_INLINE bool operator==(const local_time& lhs, const local_time& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute, lhs.second, lhs.millisecond, lhs.microsecond, lhs.nanosecond) ==
           std::make_tuple(rhs.hour, rhs.minute, rhs.second, rhs.millisecond, rhs.microsecond, rhs.nanosecond);
}
TOML11_INLINE bool operator!=(const local_time& lhs, const local_time& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_time& lhs, const local_time& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute, lhs.second, lhs.millisecond, lhs.microsecond, lhs.nanosecond) <
           std::make_tuple(rhs.hour, rhs.minute, rhs.second, rhs.millisecond, rhs.microsecond, rhs.nanosecond);
}
TOML11_INLINE bool operator<=(const local_time& lhs, const local_time& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_time& lhs, const local_time& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_time& lhs, const local_time& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_time& time)
{
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.hour  ) << ':';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.minute) << ':';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.second);
    if(time.millisecond != 0 || time.microsecond != 0 || time.nanosecond != 0)
    {
        os << '.';
        os << std::setfill('0') << std::setw(3) << static_cast<int>(time.millisecond);
        if(time.microsecond != 0 || time.nanosecond != 0)
        {
            os << std::setfill('0') << std::setw(3) << static_cast<int>(time.microsecond);
            if(time.nanosecond != 0)
            {
                os << std::setfill('0') << std::setw(3) << static_cast<int>(time.nanosecond);
            }
        }
    }
    return os;
}

TOML11_INLINE std::string to_string(const local_time& time)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << time;
    return oss.str();
}

// ----------------------------------------------------------------------------

TOML11_INLINE time_offset::operator std::chrono::minutes() const
{
    return std::chrono::minutes(this->minute) +
           std::chrono::hours(this->hour);
}

TOML11_INLINE bool operator==(const time_offset& lhs, const time_offset& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute) ==
           std::make_tuple(rhs.hour, rhs.minute);
}
TOML11_INLINE bool operator!=(const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const time_offset& lhs, const time_offset& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute) <
           std::make_tuple(rhs.hour, rhs.minute);
}
TOML11_INLINE bool operator<=(const time_offset& lhs, const time_offset& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const time_offset& offset)
{
    if(offset.hour == 0 && offset.minute == 0)
    {
        os << 'Z';
        return os;
    }
    int minute = static_cast<int>(offset.hour) * 60 + offset.minute;
    if(minute < 0){os << '-'; minute = std::abs(minute);} else {os << '+';}
    os << std::setfill('0') << std::setw(2) << minute / 60 << ':';
    os << std::setfill('0') << std::setw(2) << minute % 60;
    return os;
}

TOML11_INLINE std::string to_string(const time_offset& offset)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << offset;
    return oss.str();
}

// -----------------------------------------------------------------------------

TOML11_INLINE local_datetime::local_datetime(const std::chrono::system_clock::time_point& tp)
{
    const auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm ltime = detail::localtime_s(&t);

    this->date = local_date(ltime);
    this->time = local_time(ltime);

    // std::tm lacks subsecond information, so diff between tp and tm
    // can be used to get millisecond & microsecond information.
    const auto t_diff = tp -
        std::chrono::system_clock::from_time_t(std::mktime(&ltime));
    this->time.millisecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(t_diff).count());
    this->time.microsecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(t_diff).count());
    this->time.nanosecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds >(t_diff).count());
}

TOML11_INLINE local_datetime::local_datetime(const std::time_t t)
    : local_datetime{std::chrono::system_clock::from_time_t(t)}
{}

TOML11_INLINE local_datetime::operator std::chrono::system_clock::time_point() const
{
    using internal_duration =
        typename std::chrono::system_clock::time_point::duration;

    // Normally DST begins at A.M. 3 or 4. If we re-use conversion operator
    // of local_date and local_time independently, the conversion fails if
    // it is the day when DST begins or ends. Since local_date considers the
    // time is 00:00 A.M. and local_time does not consider DST because it
    // does not have any date information. We need to consider both date and
    // time information at the same time to convert it correctly.

    std::tm t;
    t.tm_sec   = static_cast<int>(this->time.second);
    t.tm_min   = static_cast<int>(this->time.minute);
    t.tm_hour  = static_cast<int>(this->time.hour);
    t.tm_mday  = static_cast<int>(this->date.day);
    t.tm_mon   = static_cast<int>(this->date.month);
    t.tm_year  = static_cast<int>(this->date.year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;

    // std::mktime returns date as local time zone. no conversion needed
    auto dt = std::chrono::system_clock::from_time_t(std::mktime(&t));
    dt += std::chrono::duration_cast<internal_duration>(
            std::chrono::milliseconds(this->time.millisecond) +
            std::chrono::microseconds(this->time.microsecond) +
            std::chrono::nanoseconds (this->time.nanosecond));
    return dt;
}

TOML11_INLINE local_datetime::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE bool operator==(const local_datetime& lhs, const local_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time) ==
           std::make_tuple(rhs.date, rhs.time);
}
TOML11_INLINE bool operator!=(const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_datetime& lhs, const local_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time) <
           std::make_tuple(rhs.date, rhs.time);
}
TOML11_INLINE bool operator<=(const local_datetime& lhs, const local_datetime& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_datetime& dt)
{
    os << dt.date << 'T' << dt.time;
    return os;
}

TOML11_INLINE std::string to_string(const local_datetime& dt)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << dt;
    return oss.str();
}

// -----------------------------------------------------------------------------


TOML11_INLINE offset_datetime::offset_datetime(const local_datetime& ld)
    : date{ld.date}, time{ld.time}, offset{get_local_offset(nullptr)}
      // use the current local timezone offset
{}
TOML11_INLINE offset_datetime::offset_datetime(const std::chrono::system_clock::time_point& tp)
    : offset{0, 0} // use gmtime
{
    const auto timet = std::chrono::system_clock::to_time_t(tp);
    const auto tm    = detail::gmtime_s(&timet);
    this->date = local_date(tm);
    this->time = local_time(tm);
}
TOML11_INLINE offset_datetime::offset_datetime(const std::time_t& t)
    : offset{0, 0} // use gmtime
{
    const auto tm    = detail::gmtime_s(&t);
    this->date = local_date(tm);
    this->time = local_time(tm);
}
TOML11_INLINE offset_datetime::offset_datetime(const std::tm& t)
    : offset{0, 0} // assume gmtime
{
    this->date = local_date(t);
    this->time = local_time(t);
}

TOML11_INLINE offset_datetime::operator std::chrono::system_clock::time_point() const
{
    // get date-time
    using internal_duration =
        typename std::chrono::system_clock::time_point::duration;

    // first, convert it to local date-time information in the same way as
    // local_datetime does. later we will use time_t to adjust time offset.
    std::tm t;
    t.tm_sec   = static_cast<int>(this->time.second);
    t.tm_min   = static_cast<int>(this->time.minute);
    t.tm_hour  = static_cast<int>(this->time.hour);
    t.tm_mday  = static_cast<int>(this->date.day);
    t.tm_mon   = static_cast<int>(this->date.month);
    t.tm_year  = static_cast<int>(this->date.year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;
    const std::time_t tp_loc = std::mktime(std::addressof(t));

    auto tp = std::chrono::system_clock::from_time_t(tp_loc);
    tp += std::chrono::duration_cast<internal_duration>(
            std::chrono::milliseconds(this->time.millisecond) +
            std::chrono::microseconds(this->time.microsecond) +
            std::chrono::nanoseconds (this->time.nanosecond));

    // Since mktime uses local time zone, it should be corrected.
    // `12:00:00+09:00` means `03:00:00Z`. So mktime returns `03:00:00Z` if
    // we are in `+09:00` timezone. To represent `12:00:00Z` there, we need
    // to add `+09:00` to `03:00:00Z`.
    //    Here, it uses the time_t converted from date-time info to handle
    // daylight saving time.
    const auto ofs = get_local_offset(std::addressof(tp_loc));
    tp += std::chrono::hours  (ofs.hour);
    tp += std::chrono::minutes(ofs.minute);

    // We got `12:00:00Z` by correcting local timezone applied by mktime.
    // Then we will apply the offset. Let's say `12:00:00-08:00` is given.
    // And now, we have `12:00:00Z`. `12:00:00-08:00` means `20:00:00Z`.
    // So we need to subtract the offset.
    tp -= std::chrono::minutes(this->offset);
    return tp;
}

TOML11_INLINE offset_datetime::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE time_offset offset_datetime::get_local_offset(const std::time_t* tp)
{
    // get local timezone with the same date-time information as mktime
    const auto t = detail::localtime_s(tp);

    std::array<char, 6> buf;
    const auto result = std::strftime(buf.data(), 6, "%z", &t); // +hhmm\0
    if(result != 5)
    {
        throw std::runtime_error("toml::offset_datetime: cannot obtain "
                                 "timezone information of current env");
    }
    const int ofs = std::atoi(buf.data());
    const int ofs_h = ofs / 100;
    const int ofs_m = ofs - (ofs_h * 100);
    return time_offset(ofs_h, ofs_m);
}

TOML11_INLINE bool operator==(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time, lhs.offset) ==
           std::make_tuple(rhs.date, rhs.time, rhs.offset);
}
TOML11_INLINE bool operator!=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const offset_datetime& lhs, const offset_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time, lhs.offset) <
           std::make_tuple(rhs.date, rhs.time, rhs.offset);
}
TOML11_INLINE bool operator<=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const offset_datetime& dt)
{
    os << dt.date << 'T' << dt.time << dt.offset;
    return os;
}

TOML11_INLINE std::string to_string(const offset_datetime& dt)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << dt;
    return oss.str();
}

}//toml
#endif // TOML11_DATETIME_IMPL_HPP
