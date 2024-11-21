#ifndef TOML11_VALUE_T_IMPL_HPP
#define TOML11_VALUE_T_IMPL_HPP

#include "../fwd/value_t_fwd.hpp"

#include <ostream>
#include <sstream>
#include <string>

namespace toml
{

TOML11_INLINE std::ostream& operator<<(std::ostream& os, value_t t)
{
    switch(t)
    {
        case value_t::boolean         : os << "boolean";         return os;
        case value_t::integer         : os << "integer";         return os;
        case value_t::floating        : os << "floating";        return os;
        case value_t::string          : os << "string";          return os;
        case value_t::offset_datetime : os << "offset_datetime"; return os;
        case value_t::local_datetime  : os << "local_datetime";  return os;
        case value_t::local_date      : os << "local_date";      return os;
        case value_t::local_time      : os << "local_time";      return os;
        case value_t::array           : os << "array";           return os;
        case value_t::table           : os << "table";           return os;
        case value_t::empty           : os << "empty";           return os;
        default                       : os << "unknown";         return os;
    }
}

TOML11_INLINE std::string to_string(value_t t)
{
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

} // namespace toml
#endif // TOML11_VALUE_T_IMPL_HPP
