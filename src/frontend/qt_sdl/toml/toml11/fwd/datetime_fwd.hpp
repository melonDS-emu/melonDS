#ifndef TOML11_DATETIME_FWD_HPP
#define TOML11_DATETIME_FWD_HPP

#include <chrono>
#include <iosfwd>
#include <string>

#include <cstdint>
#include <cstdlib>
#include <ctime>

namespace toml
{

enum class month_t : std::uint8_t
{
    Jan =  0,
    Feb =  1,
    Mar =  2,
    Apr =  3,
    May =  4,
    Jun =  5,
    Jul =  6,
    Aug =  7,
    Sep =  8,
    Oct =  9,
    Nov = 10,
    Dec = 11
};

// ----------------------------------------------------------------------------

struct local_date
{
    std::int16_t year{0};   // A.D. (like, 2018)
    std::uint8_t month{0};  // [0, 11]
    std::uint8_t day{0};    // [1, 31]

    local_date(int y, month_t m, int d)
        : year {static_cast<std::int16_t>(y)},
          month{static_cast<std::uint8_t>(m)},
          day  {static_cast<std::uint8_t>(d)}
    {}

    explicit local_date(const std::tm& t)
        : year {static_cast<std::int16_t>(t.tm_year + 1900)},
          month{static_cast<std::uint8_t>(t.tm_mon)},
          day  {static_cast<std::uint8_t>(t.tm_mday)}
    {}

    explicit local_date(const std::chrono::system_clock::time_point& tp);
    explicit local_date(const std::time_t t);

    operator std::chrono::system_clock::time_point() const;
    operator std::time_t() const;

    local_date() = default;
    ~local_date() = default;
    local_date(local_date const&) = default;
    local_date(local_date&&)      = default;
    local_date& operator=(local_date const&) = default;
    local_date& operator=(local_date&&)      = default;
};
bool operator==(const local_date& lhs, const local_date& rhs);
bool operator!=(const local_date& lhs, const local_date& rhs);
bool operator< (const local_date& lhs, const local_date& rhs);
bool operator<=(const local_date& lhs, const local_date& rhs);
bool operator> (const local_date& lhs, const local_date& rhs);
bool operator>=(const local_date& lhs, const local_date& rhs);

std::ostream& operator<<(std::ostream& os, const local_date& date);
std::string to_string(const local_date& date);

// -----------------------------------------------------------------------------

struct local_time
{
    std::uint8_t  hour{0};        // [0, 23]
    std::uint8_t  minute{0};      // [0, 59]
    std::uint8_t  second{0};      // [0, 60]
    std::uint16_t millisecond{0}; // [0, 999]
    std::uint16_t microsecond{0}; // [0, 999]
    std::uint16_t nanosecond{0};  // [0, 999]

    local_time(int h, int m, int s,
               int ms = 0, int us = 0, int ns = 0)
        : hour  {static_cast<std::uint8_t>(h)},
          minute{static_cast<std::uint8_t>(m)},
          second{static_cast<std::uint8_t>(s)},
          millisecond{static_cast<std::uint16_t>(ms)},
          microsecond{static_cast<std::uint16_t>(us)},
          nanosecond {static_cast<std::uint16_t>(ns)}
    {}

    explicit local_time(const std::tm& t)
        : hour  {static_cast<std::uint8_t>(t.tm_hour)},
          minute{static_cast<std::uint8_t>(t.tm_min )},
          second{static_cast<std::uint8_t>(t.tm_sec )},
          millisecond{0}, microsecond{0}, nanosecond{0}
    {}

    template<typename Rep, typename Period>
    explicit local_time(const std::chrono::duration<Rep, Period>& t)
    {
        const auto h = std::chrono::duration_cast<std::chrono::hours>(t);
        this->hour = static_cast<std::uint8_t>(h.count());
        const auto t2 = t - h;
        const auto m = std::chrono::duration_cast<std::chrono::minutes>(t2);
        this->minute = static_cast<std::uint8_t>(m.count());
        const auto t3 = t2 - m;
        const auto s = std::chrono::duration_cast<std::chrono::seconds>(t3);
        this->second = static_cast<std::uint8_t>(s.count());
        const auto t4 = t3 - s;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t4);
        this->millisecond = static_cast<std::uint16_t>(ms.count());
        const auto t5 = t4 - ms;
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t5);
        this->microsecond = static_cast<std::uint16_t>(us.count());
        const auto t6 = t5 - us;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t6);
        this->nanosecond = static_cast<std::uint16_t>(ns.count());
    }

    operator std::chrono::nanoseconds() const;

    local_time() = default;
    ~local_time() = default;
    local_time(local_time const&) = default;
    local_time(local_time&&)      = default;
    local_time& operator=(local_time const&) = default;
    local_time& operator=(local_time&&)      = default;
};

bool operator==(const local_time& lhs, const local_time& rhs);
bool operator!=(const local_time& lhs, const local_time& rhs);
bool operator< (const local_time& lhs, const local_time& rhs);
bool operator<=(const local_time& lhs, const local_time& rhs);
bool operator> (const local_time& lhs, const local_time& rhs);
bool operator>=(const local_time& lhs, const local_time& rhs);

std::ostream& operator<<(std::ostream& os, const local_time& time);
std::string to_string(const local_time& time);

// ----------------------------------------------------------------------------

struct time_offset
{
    std::int8_t hour{0};   // [-12, 12]
    std::int8_t minute{0}; // [-59, 59]

    time_offset(int h, int m)
        : hour  {static_cast<std::int8_t>(h)},
          minute{static_cast<std::int8_t>(m)}
    {}

    operator std::chrono::minutes() const;

    time_offset() = default;
    ~time_offset() = default;
    time_offset(time_offset const&) = default;
    time_offset(time_offset&&)      = default;
    time_offset& operator=(time_offset const&) = default;
    time_offset& operator=(time_offset&&)      = default;
};

bool operator==(const time_offset& lhs, const time_offset& rhs);
bool operator!=(const time_offset& lhs, const time_offset& rhs);
bool operator< (const time_offset& lhs, const time_offset& rhs);
bool operator<=(const time_offset& lhs, const time_offset& rhs);
bool operator> (const time_offset& lhs, const time_offset& rhs);
bool operator>=(const time_offset& lhs, const time_offset& rhs);

std::ostream& operator<<(std::ostream& os, const time_offset& offset);

std::string to_string(const time_offset& offset);

// -----------------------------------------------------------------------------

struct local_datetime
{
    local_date date{};
    local_time time{};

    local_datetime(local_date d, local_time t): date{d}, time{t} {}

    explicit local_datetime(const std::tm& t): date{t}, time{t}{}

    explicit local_datetime(const std::chrono::system_clock::time_point& tp);
    explicit local_datetime(const std::time_t t);

    operator std::chrono::system_clock::time_point() const;
    operator std::time_t() const;

    local_datetime() = default;
    ~local_datetime() = default;
    local_datetime(local_datetime const&) = default;
    local_datetime(local_datetime&&)      = default;
    local_datetime& operator=(local_datetime const&) = default;
    local_datetime& operator=(local_datetime&&)      = default;
};

bool operator==(const local_datetime& lhs, const local_datetime& rhs);
bool operator!=(const local_datetime& lhs, const local_datetime& rhs);
bool operator< (const local_datetime& lhs, const local_datetime& rhs);
bool operator<=(const local_datetime& lhs, const local_datetime& rhs);
bool operator> (const local_datetime& lhs, const local_datetime& rhs);
bool operator>=(const local_datetime& lhs, const local_datetime& rhs);

std::ostream& operator<<(std::ostream& os, const local_datetime& dt);

std::string to_string(const local_datetime& dt);

// -----------------------------------------------------------------------------

struct offset_datetime
{
    local_date  date{};
    local_time  time{};
    time_offset offset{};

    offset_datetime(local_date d, local_time t, time_offset o)
        : date{d}, time{t}, offset{o}
    {}
    offset_datetime(const local_datetime& dt, time_offset o)
        : date{dt.date}, time{dt.time}, offset{o}
    {}
    // use the current local timezone offset
    explicit offset_datetime(const local_datetime& ld);
    explicit offset_datetime(const std::chrono::system_clock::time_point& tp);
    explicit offset_datetime(const std::time_t& t);
    explicit offset_datetime(const std::tm& t);

    operator std::chrono::system_clock::time_point() const;

    operator std::time_t() const;

    offset_datetime() = default;
    ~offset_datetime() = default;
    offset_datetime(offset_datetime const&) = default;
    offset_datetime(offset_datetime&&)      = default;
    offset_datetime& operator=(offset_datetime const&) = default;
    offset_datetime& operator=(offset_datetime&&)      = default;

  private:

    static time_offset get_local_offset(const std::time_t* tp);
};

bool operator==(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator!=(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator< (const offset_datetime& lhs, const offset_datetime& rhs);
bool operator<=(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator> (const offset_datetime& lhs, const offset_datetime& rhs);
bool operator>=(const offset_datetime& lhs, const offset_datetime& rhs);

std::ostream& operator<<(std::ostream& os, const offset_datetime& dt);

std::string to_string(const offset_datetime& dt);

}//toml
#endif // TOML11_DATETIME_FWD_HPP
