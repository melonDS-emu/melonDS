#ifndef TOML11_COLOR_FWD_HPP
#define TOML11_COLOR_FWD_HPP

#include <iosfwd>

#ifdef TOML11_COLORIZE_ERROR_MESSAGE
#define TOML11_ERROR_MESSAGE_COLORIZED true
#else
#define TOML11_ERROR_MESSAGE_COLORIZED false
#endif

#ifdef TOML11_USE_THREAD_LOCAL_COLORIZATION
#define TOML11_THREAD_LOCAL_COLORIZATION thread_local
#else
#define TOML11_THREAD_LOCAL_COLORIZATION
#endif

namespace toml
{
namespace color
{
// put ANSI escape sequence to ostream
inline namespace ansi
{
namespace detail
{

// Control color mode globally
class color_mode
{
  public:

    void enable() noexcept
    {
        should_color_ = true;
    }
    void disable() noexcept
    {
        should_color_ = false;
    }
    bool should_color() const noexcept
    {
        return should_color_;
    }

  private:

    bool should_color_ = TOML11_ERROR_MESSAGE_COLORIZED;
};

inline color_mode& color_status() noexcept
{
    static TOML11_THREAD_LOCAL_COLORIZATION color_mode status;
    return status;
}

} // detail

std::ostream& reset  (std::ostream& os);
std::ostream& bold   (std::ostream& os);
std::ostream& grey   (std::ostream& os);
std::ostream& gray   (std::ostream& os);
std::ostream& red    (std::ostream& os);
std::ostream& green  (std::ostream& os);
std::ostream& yellow (std::ostream& os);
std::ostream& blue   (std::ostream& os);
std::ostream& magenta(std::ostream& os);
std::ostream& cyan   (std::ostream& os);
std::ostream& white  (std::ostream& os);

} // ansi

inline void enable()
{
    return detail::color_status().enable();
}
inline void disable()
{
    return detail::color_status().disable();
}
inline bool should_color()
{
    return detail::color_status().should_color();
}

} // color
} // toml
#endif // TOML11_COLOR_FWD_HPP
