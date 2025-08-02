#ifndef TOML11_COMMENTS_IMPL_HPP
#define TOML11_COMMENTS_IMPL_HPP

#include "../fwd/comments_fwd.hpp" // IWYU pragma: keep

namespace toml
{

TOML11_INLINE bool operator==(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments == rhs.comments;}
TOML11_INLINE bool operator!=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments != rhs.comments;}
TOML11_INLINE bool operator< (const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments <  rhs.comments;}
TOML11_INLINE bool operator<=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments <= rhs.comments;}
TOML11_INLINE bool operator> (const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments >  rhs.comments;}
TOML11_INLINE bool operator>=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments >= rhs.comments;}

TOML11_INLINE void swap(preserve_comments& lhs, preserve_comments& rhs)
{
    lhs.swap(rhs);
    return;
}
TOML11_INLINE void swap(preserve_comments& lhs, std::vector<std::string>& rhs)
{
    lhs.comments.swap(rhs);
    return;
}
TOML11_INLINE void swap(std::vector<std::string>& lhs, preserve_comments& rhs)
{
    lhs.swap(rhs.comments);
    return;
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const preserve_comments& com)
{
    for(const auto& c : com)
    {
        if(c.front() != '#')
        {
            os << '#';
        }
        os << c << '\n';
    }
    return os;
}

} // toml11
#endif // TOML11_COMMENTS_IMPL_HPP
